#ifndef HLCUP_CONNECTION_HPP__
#define HLCUP_CONNECTION_HPP__

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <sstream>

#include <sys/mman.h>
#include <sys/sendfile.h>

#include "Storage.hpp"
#include "http_parser.hpp"
#include "itoa_branchlut.hpp"

#include "Writer.hpp"

namespace hlcup {

class Connection {
    using Action = Request::Action;

    static constexpr const char *ok_header =
        "HTTP/1.1 200 OK\r\n"
        "Connection: keep-alive\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: ";
    static constexpr const char *gender_m = "\"gender\":\"m\"";
    static constexpr const char *gender_f = "\"gender\":\"f\"";

    static constexpr const char *err_headers =
        "Content-Type: text/plain\r\n"
        "Content-Length: 0\r\n"
        "Connection: keep-alive\r\n";

    static constexpr const IoVec post_ok{
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}"};

    static constexpr const IoVec err_400_tpl[] = {"HTTP/1.1 400 Bad Request\r\n", err_headers, "\r\n"};
    static constexpr const IoVec err_404_tpl[] = {"HTTP/1.1 404 Not Found\r\n", err_headers, "\r\n"};

    //    static constexpr std::array<IoVec, 16> users_tpl{ok_header,  nullptr,
    //                                                     "\r\n\r\n", "{\"id\":",
    //                                                     nullptr,    ",\"first_name\":\"",
    //                                                     nullptr,    "\",\"last_name\":\"",
    //                                                     nullptr,    "\",\"email\":\"",
    //                                                     nullptr,    "\",\"birth_date\":",
    //                                                     nullptr,    ",",
    //                                                     gender_m,   "}"};

    static constexpr const char *marks[] = {
        ",\"mark\":0}", ",\"mark\":1}", ",\"mark\":2}", ",\"mark\":3}", ",\"mark\":4}", ",\"mark\":5}",
    };

    bool closed_ = false;

public:
    Connection(int fd) : sock_fd_(fd) {}

    ~Connection() {
        if (!closed_) {
            ::close(sock_fd_);
        }
    }

    ssize_t write(const void *buf, size_t size) { return ::write(sock_fd_, buf, size); }
    ssize_t read(void *buf, size_t size) { return ::read(sock_fd_, buf, size); }
    int fd() const { return sock_fd_; }

    inline void writeBadRequest() const { ::writev(sock_fd_, err_400_tpl, get_size(err_400_tpl)); }
    inline void writeNotFound() const { ::writev(sock_fd_, err_404_tpl, get_size(err_404_tpl)); }

    inline void enableWriters() const {
        if (!Writer::working) {
            Writer::working = true;
            std::lock_guard<std::mutex> lock(Writer::mutex);
            Writer::cond.notify_all();
        }
    }

    inline void disableWriters() const {
        if (Writer::working) {
            Writer::working = false;
        }
    }

    inline void writeUser(int32_t id) const {
        disableWriters();

        Storage &s = Storage::get();
        if (!s.userExists(id)) {
            writeNotFound();
            return;
        }
        Storage::UserItem &u = s.users[id];

        char id_buf[10] = {0};
        char clen_buf[10] = {0};
        char bdate_buf[16] = {0};
        IoVec tpl[]{ok_header,
                    nullptr,
                    "\r\n\r\n",
                    "{\"id\":",
                    id_buf,
                    ",\"first_name\":\"",
                    s.first_names[u.first_name_id],
                    "\",\"last_name\":\"",
                    s.last_names[u.last_name_id],
                    "\",\"email\":\"",
                    u.email,
                    "\",\"birth_date\":",
                    bdate_buf,
                    ",",
                    gender_m,
                    "}"};

        //        tpl[4].iov_base = id_buf;
        tpl[4].iov_len = u32toa_branchlut2(static_cast<uint32_t>(u.id), id_buf) - id_buf;
        //        tpl[6].iov_base = u.first_name.data();
        //        tpl[6].iov_len = u.first_name.size();
        //        tpl[8].iov_base = u.last_name.data();
        //        tpl[8].iov_len = u.last_name.size();
        //        tpl[10].iov_base = u.email.data();
        //        tpl[10].iov_len = u.email.size();
        //        tpl[12].iov_base = bdate_buf;
        tpl[12].iov_len = i32toa_branchlut2(u.birth_date, bdate_buf) - bdate_buf;
        if (u.gender == Gender::kFemale) {
            tpl[14].iov_base = (char *)gender_f;
        }

        unsigned int clen = 0;
        for (size_t i = 3; i < get_size(tpl); i++) {
            clen += tpl[i].iov_len;
        }

        tpl[1].iov_base = clen_buf;
        tpl[1].iov_len = u32toa_branchlut2(clen, clen_buf) - clen_buf;

        ::writev(sock_fd_, tpl, get_size(tpl));
    }

    inline void writeLocation(int32_t id) const {
        disableWriters();

        Storage &s = Storage::get();
        if (!s.locationExists(id)) {
            writeNotFound();
            return;
        }
        Storage::LocationItem &l = s.locations[id];

        char clen_buf[10] = {0};
        char id_buf[10] = {0};
        char distance_buf[16] = {0};
        IoVec tpl[]{ok_header,
                    clen_buf,
                    "\r\n\r\n",
                    "{\"id\":",
                    id_buf,
                    ",\"distance\":",
                    distance_buf,
                    ",\"place\":\"",
                    s.places[l.place_id],
                    "\",\"country\":\"",
                    s.countries[l.country_id],
                    "\",\"city\":\"",
                    s.cities[l.city_id],
                    "\"}"};

        tpl[4].iov_len = i32toa_branchlut2((unsigned)l.id, id_buf) - id_buf;
        tpl[6].iov_len = i32toa_branchlut2((unsigned)l.distance, distance_buf) - distance_buf;

        unsigned int clen = 0;
        for (size_t i = 3; i < get_size(tpl); i++) {
            clen += tpl[i].iov_len;
        }

        tpl[1].iov_len = u32toa_branchlut2(clen, clen_buf) - clen_buf;

        ::writev(sock_fd_, tpl, get_size(tpl));
    }

    inline void writeVisit(int32_t id) const {
        disableWriters();

        Storage &s = Storage::get();
        if (!s.visitExists(id)) {
            writeNotFound();
            return;
        }
        Storage::VisitItem &v = s.visits[id];

        char clen_buf[10] = {0};
        char id_buf[10] = {0};
        char loc_buf[10] = {0};
        char user_buf[10] = {0};
        char time_buf[16] = {0};
        IoVec tpl[]{ok_header, clen_buf,     "\r\n\r\n", "{\"id\":",         id_buf,   ",\"location\":",
                    loc_buf,   ",\"user\":", user_buf,   ",\"visited_at\":", time_buf, marks[v.mark]};

        tpl[4].iov_len = i32toa_branchlut2((unsigned)v.id, id_buf) - id_buf;
        tpl[6].iov_len = i32toa_branchlut2((unsigned)v.location_id, loc_buf) - loc_buf;
        tpl[8].iov_len = i32toa_branchlut2((unsigned)v.user_id, user_buf) - user_buf;
        tpl[10].iov_len = i32toa_branchlut2((unsigned)v.visited_at, time_buf) - time_buf;

        unsigned int clen = 0;
        for (size_t i = 3; i < get_size(tpl); i++) {
            clen += tpl[i].iov_len;
        }

        tpl[1].iov_base = clen_buf;
        tpl[1].iov_len = u32toa_branchlut2(clen, clen_buf) - clen_buf;

        ::writev(sock_fd_, tpl, get_size(tpl));
    }

    inline void writePostOk() {
        ::write(sock_fd_, post_ok.iov_base, post_ok.iov_len);
        ::close(sock_fd_);
        closed_ = true;
    }

    inline void insertUser(Request &&r) {
        enableWriters();

        Storage &s = Storage::get();
        if (s.userExists(r.entity_id) || !r.u.mask.all()) {
            writeBadRequest();
        } else {
            writePostOk();
        }

        //        Storage::UserItem &it = s.users[r.u.id];
        //        it.id = r.u.id;
        //        it.gender = r.u.gender;
        //        it.birth_date = r.u.birth_date;
        //        it.age = s.userAge(it);
        //        it.email = std::move(r.u.email);
        //        {
        //            auto iter = s.first_names_map.find(r.u.first_name);
        //            if (iter == s.first_names_map.end()) {
        //                Writer::Strings::get().addFirstName(it.id, r.u.first_name);
        //            } else {
        //                it.first_name_id = iter->second;
        //            }
        //        }

        //        {
        //            auto iter = s.last_names_map.find(r.u.last_name);
        //            if (iter == s.last_names_map.end()) {
        //                Writer::Strings::get().addLastName(it.id, r.u.last_name);
        //            } else {
        //                it.last_name_id = iter->second;
        //            }
        //        }

        Writer::Users::get(r.u.id).update(std::move(r.u));
    }

    inline void insertLocation(Request &&r) {
        enableWriters();

        Storage &s = Storage::get();
        if (s.locationExists(r.entity_id) || !r.l.mask.all()) {
            writeBadRequest();
        } else {
            writePostOk();
            ::close(sock_fd_);
            closed_ = true;
        }

        //        Storage::LocationItem &it = s.locations[r.l.id];
        //        it.id = r.l.id;
        //        it.distance = r.l.distance;
        //        {
        //            auto iter = s.cities_map.find(r.l.city);
        //            if (iter == s.cities_map.end()) {
        //                Writer::Strings::get().addCity(it.id, r.l.city);
        //            } else {
        //                it.city_id = iter->second;
        //            }
        //        }

        //        {
        //            auto iter = s.countries_map.find(r.l.country);
        //            if (iter == s.countries_map.end()) {
        //                Writer::Strings::get().addCountry(it.id, r.l.country);
        //            } else {
        //                it.country_id = iter->second;
        //            }
        //        }

        //        {
        //            auto iter = s.places_map.find(r.l.place);
        //            if (iter == s.places_map.end()) {
        //                Writer::Strings::get().addPlace(it.id, r.l.place);
        //            } else {
        //                it.place_id = iter->second;
        //            }
        //        }
        Writer::Locations::get(r.l.id).update(std::move(r.l));
    }

    inline void insertVisit(Request &&r) {
        enableWriters();

        Storage &s = Storage::get();
        if (s.visitExists(r.entity_id) || !r.v.mask.all()) {
            writeBadRequest();
        } else {
            writePostOk();
            ::close(sock_fd_);
            closed_ = true;
        }

        Writer::Visits::get(r.v.id).update(r.v);
        //        Writer::UserVisits::get(r.v.user_id).add(r.v.user_id, r.v.id);
        //        Writer::LocationsVisits::get(r.v.location_id).add(r.v.location_id, r.v.id);
        //        s.visits[r.v.id] = r.v;
    }

    inline void updateUser(Request &&r) {
        enableWriters();

        Storage &s = Storage::get();
        if (!s.userExists(r.entity_id)) {
            writeNotFound();
        } else if (r.u.mask.test(User::kId)) {
            writeBadRequest();
        } else {
            writePostOk();
        }

        r.u.id = r.entity_id;
        Writer::Users::get(r.entity_id).update(std::move(r.u));
    }

    inline void updateLocation(Request &r) {
        enableWriters();

        Storage &s = Storage::get();
        if (!s.locationExists(r.entity_id)) {
            writeNotFound();
        } else if (r.l.mask.test(Location::kId)) {
            writeBadRequest();
        } else {
            writePostOk();
        }

        r.l.id = r.entity_id;
        Writer::Locations::get(r.entity_id).update(std::move(r.l));
    }

    inline void updateVisit(Request &r) {
        enableWriters();

        Storage &s = Storage::get();
        if (!s.visitExists(r.entity_id)) {
            writeNotFound();
        } else if (r.v.mask.test(Visit::kId)) {
            writeBadRequest();
        } else {
            writePostOk();
        }

        //        using P = Visit::Param;
        //        Storage::VisitItem &it = s.visits[r.v.id];
        //        if (r.v.mask.test(P::kMark)) {
        //            it.mark = r.v.mark;
        //        }
        //        if (r.v.mask.test(P::kUserId)) {
        //            Writer::UserVisits::get(it.user_id).remove(it.user_id, it.id);
        //            it.user_id = r.v.user_id;
        //            Writer::UserVisits::get(it.user_id).add(it.user_id, it.id);
        //        }
        //        if (r.v.mask.test(P::kLocationId)) {
        //            Writer::LocationsVisits::get(it.location_id).remove(it.location_id, it.id);
        //            it.location_id = r.v.location_id;
        //            Writer::LocationsVisits::get(it.location_id).add(it.location_id, it.id);
        //        }
        //        if (r.v.mask.test(P::kVisitedAt)) {
        //            it.visited_at = r.v.visited_at;
        //            Writer::UserVisits::get(it.user_id).sort(it.user_id, it.id);
        //            Writer::LocationsVisits::get(it.location_id).sort(it.location_id, it.id);
        //        }

        r.v.id = r.entity_id;
        Writer::Visits::get(r.entity_id).update(r.v);
    }

    void writeUserVisits(Request &r) {
        disableWriters();

        Storage &s = Storage::get();
        if (!s.userExists(r.entity_id)) {
            writeNotFound();
            return;
        }
        //         char clen_buf[10] = {0};
        //         std::vector <IoVec> tpl{ok_header, clen_buf, "\r\n\r\n", "{visits:["};
        using Param = Request::Param;
        //        for (int i = 0; i < s.countries.size(); i++) {
        //            std::cout << i << " " << s.countries[i] << std::endl;
        //        }
        int cid = Entity::kInvalidId;
        if (r.isSet(Param::kCountry)) {
            if (s.countries_map.find(r.country) != s.countries_map.end()) {
                cid = s.countries_map.at(r.country);
            }
        }

        std::string body = "{\"visits\":[ ";
        for (int32_t vid : s.users[r.entity_id].visits) {
            if (vid == Entity::kInvalidId) continue;
            Storage::VisitItem &v = s.visits[vid];
            Storage::LocationItem &l = s.locations[v.location_id];

            if (r.isSet(Param::kFromDate)) {
                if (v.visited_at <= r.from_date) continue;
            }
            if (r.isSet(Param::kToDate)) {
                if (v.visited_at >= r.to_date) continue;
            }

            if (r.isSet(Param::kCountry)) {
                if (l.country_id != cid) continue;
            }
            if (r.isSet(Param::kToDistance)) {
                if (l.distance >= r.to_distance) continue;
            }

            body += "{\"mark\":" + std::to_string((int)v.mark) + ",\"visited_at\":" + std::to_string(v.visited_at) + ",\"place\":\"" + s.places[l.place_id] +
                    "\"},";
        }

        body[body.size() - 1] = ' ';
        body += "]}";
        std::string resp = ok_header + std::to_string(body.size()) + "\r\n\r\n" + body;

        ::write(sock_fd_, resp.data(), resp.size());
    }

    void writeLocationAvg(Request &r) {
        disableWriters();

        Storage &s = Storage::get();
        int32_t lid = r.entity_id;

        if (!s.locationExists(lid)) {
            writeNotFound();
            return;
        }

        using Param = Request::Param;
        uint64_t sum = 0;
        uint32_t count = 0;
        for (int32_t vid : s.locations[lid].visits) {
            Storage::VisitItem &v = s.visits[vid];

            if (r.isSet(Param::kFromDate)) {
                if (!(v.visited_at > r.from_date)) continue;
            }
            if (r.isSet(Param::kToDate)) {
                if (!(v.visited_at < r.to_date)) continue;
            }

            if (r.isSet(Param::kGender)) {
                if (!(r.gender == s.users[v.user_id].gender)) continue;
                //                if ((s.users[v.user_id] >> 7) != (unsigned char)r.gender) continue;
            }
            if (r.isSet(Param::kFromAge)) {
                //                if (!(s.users_ages[v.user_id] > r.from_age)) continue;
                //                if (s.users[v.user_id].birth_date >= static_cast<int64_t>(s.current_time) - static_cast<int64_t>(r.from_age) *
                //                Storage::kSecondsInYear)
                //                    continue;
                if ((s.users[v.user_id].age) < r.from_age) continue;
            }
            if (r.isSet(Param::kToAge)) {
                //                if ((static_cast<int64_t>(s.current_time) - s.users[v.user_id].birth_date) / Storage::kSecondsInYear >=
                //                static_cast<int64_t>(r.to_age)) continue;
                if ((s.users[v.user_id].age) >= r.to_age) continue;
            }

            sum += v.mark;
            count++;
        }

        //        std::string body;
        //        if (count > 0) {
        //            double avg = 1.0 * sum / (1.0 * count);
        //            std::stringstream ss;
        //            ss << "{\"avg\":" << std::setprecision(6) << avg << "}";
        //            body = ss.str();
        //        } else {
        //            body = "{\"avg\":0}";
        //        }

        char body[] = {'{', '"', 'a', 'v', 'g', '"', ':', '0', '.', '0', '0', '0', '0', '0', '}'};
        fast_div_write(body + 7, sum, count);
        IoVec tpl[]{ok_header, "15\r\n\r\n", body};

        ::writev(sock_fd_, tpl, get_size(tpl));
    }

    void handleRequest(Request &&r) {
        switch (r.action) {
        case Action::kBadRequest: writeBadRequest(); break;
        case Action::kNotFound: writeNotFound(); break;
        case Action::kGetUser: writeUser(r.entity_id); break;
        case Action::kGetLocation: writeLocation(r.entity_id); break;
        case Action::kGetVisit: writeVisit(r.entity_id); break;
        case Action::kGetUserVisits: writeUserVisits(r); break;
        case Action::kGetLocationAvg: writeLocationAvg(r); break;
        case Action::kPostLocation: updateLocation(r); break;
        case Action::kPostLocationNew: insertLocation(std::move(r)); break;
        case Action::kPostUser: updateUser(std::move(r)); break;
        case Action::kPostUserNew: insertUser(std::move(r)); break;
        case Action::kPostVisit: updateVisit(r); break;
        case Action::kPostVisitNew: insertVisit(std::move(r)); break;
        default: writeBadRequest();
        }
    }

    bool handleData(const char *buf, size_t size) {
        const char *buf_end = buf + size;
        while (buf != buf_end) {
            buf = parser.parse(buf, buf_end);
            if (parser.done) {
                handleRequest(std::move(parser.r));
                if (parser.connection_close) {
                    return true;
                }
                parser.reset();
            }
        }
        return false;
    }

private:
    int sock_fd_;
    HttpParser parser;
};
}

#endif
