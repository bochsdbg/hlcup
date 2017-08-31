#ifndef HLCUP_WRITER_HPP__
#define HLCUP_WRITER_HPP__

#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Request.hpp"
#include "Storage.hpp"
#include "common.hpp"
#include "concurrentqueue.h"
#include "lockfree.hpp"

namespace hlcup {

struct Writer {
    static const size_t kMaxWriters = 1;

    //    static std::atomic<bool> working;
    static bool working;
    static std::condition_variable cond;
    static std::mutex mutex;

    template <typename Item>
    struct Worker {
        std::thread *thread;

        //        boost::lockfree::spsc_queue<Item, boost::lockfree::capacity<1024 * 1024>> queue;
        //        mpmc_bounded_queue<Item> queue;
        ::moodycamel::ConcurrentQueue<Item> queue;
    };

    struct Strings {
        using Item = std::tuple<char, int32_t, std::string>;
        boost::lockfree::spsc_queue<Item, boost::lockfree::capacity<1024 * 1024>> queue;

        std::thread *thread;
        Strings() {
            //            thread = new std::thread([this] { run(); });
        }
        ~Strings() { delete thread; }

        static Strings self;
        static Strings &get() { return self; }

        void addPlace(int32_t id, const std::string &s) { queue.push(std::make_tuple(0, id, s)); }
        void addCountry(int32_t id, const std::string &s) { queue.push(std::make_tuple(1, id, s)); }
        void addCity(int32_t id, const std::string &s) { queue.push(std::make_tuple(2, id, s)); }
        void addFirstName(int32_t id, const std::string &s) { queue.push(std::make_tuple(3, id, s)); }
        void addLastName(int32_t id, const std::string &s) { queue.push(std::make_tuple(4, id, s)); }

        void run() {
            for (;;) {
                std::unique_lock<std::mutex> lock(Writer::mutex);
                Writer::cond.wait(lock);

                while (Writer::working) {
                    Item p;
                    if (queue.pop(&p)) {
                        Storage &s = Storage::get();
                        char act;
                        int32_t eid;
                        std::string str;
                        std::tie(act, eid, str) = p;
                        if (act == 0) {
                            s.locations[eid].country_id = s.find_or_create_in_dict(std::move(str), s.countries, s.countries_map);
                        } else if (act == 1) {
                            s.locations[eid].place_id = s.find_or_create_in_dict(std::move(str), s.places, s.places_map);
                        } else if (act == 2) {
                            s.locations[eid].city_id = s.find_or_create_in_dict(std::move(str), s.cities, s.cities_map);
                        } else if (act == 4) {
                            s.users[eid].first_name_id = s.find_or_create_in_dict(std::move(str), s.first_names, s.first_names_map);
                        } else if (act == 5) {
                            s.users[eid].last_name_id = s.find_or_create_in_dict(std::move(str), s.last_names, s.last_names_map);
                        }

                    } else {
                        ::usleep(100);
                    }
                }
            }
        }
    };

    struct UserVisits : public Worker<std::tuple<char, int32_t, int32_t>> {
        //        using Item = ;
        //        boost::lockfree::spsc_queue<Item, boost::lockfree::capacity<1024 * 1024>> queue;

        std::thread *thread;
        UserVisits() {
            //            thread = new std::thread([this] { run(); });
        }
        ~UserVisits() { delete thread; }

        static UserVisits self[Writer::kMaxWriters];
        static UserVisits &get(uint32_t i) { return self[i % kMaxWriters]; }

        void add(int32_t uid, int32_t vid) { queue.enqueue(std::make_tuple(0, uid, vid)); }
        void remove(int32_t uid, int32_t vid) { queue.enqueue(std::make_tuple(1, uid, vid)); }
        void sort(int32_t uid, int32_t vid) { queue.enqueue(std::make_tuple(2, uid, vid)); }

        void run() {
            //            Storage &s = Storage::get();
            for (;;) {
                //                std::unique_lock<std::mutex> lock(Writer::mutex);
                //                Writer::cond.wait(lock);

                while (Writer::working) {
                    Storage &s = Storage::get();
                    std::tuple<char, int32_t, int32_t> p;
                    if (queue.try_dequeue(p)) {
                        char act;
                        int32_t uid, vid;
                        std::tie(act, uid, vid) = p;
                        if (act == 0) {
                            insert_sorted(s.users[uid].visits, vid, Storage::VisitAtCmp());
                        } else if (act == 1) {
                            erase_sorted(s.users[uid].visits, vid, Storage::VisitAtCmp());
                        } else if (act == 2) {
                            std::sort(s.users[uid].visits.begin(), s.users[uid].visits.end(), Storage::VisitAtCmp());
                        }
                    }
                }
                ::usleep(100);
            }
        }
    };

    struct LocationsVisits : Worker<std::tuple<char, int32_t, int32_t>> {
        using Item = std::tuple<char, int32_t, int32_t>;
        //        boost::lockfree::spsc_queue<Item, boost::lockfree::capacity<1024 * 1024>> queue;

        std::thread *thread;
        LocationsVisits() {
            //            thread = new std::thread([this] { run(); });
        }
        ~LocationsVisits() { delete thread; }

        static LocationsVisits self[Writer::kMaxWriters];
        static LocationsVisits &get(uint32_t i) { return self[i % Writer::kMaxWriters]; }

        void add(int32_t lid, int32_t vid) { queue.enqueue(std::make_tuple(0, lid, vid)); }
        void remove(int32_t lid, int32_t vid) { queue.enqueue(std::make_tuple(1, lid, vid)); }
        void sort(int32_t lid, int32_t vid) { queue.enqueue(std::make_tuple(2, lid, vid)); }

        void run() {
            for (;;) {
                //                std::unique_lock<std::mutex> lock(Writer::mutex);
                //                Writer::cond.wait(lock);

                while (Writer::working) {
                    Item p;
                    if (queue.try_dequeue(p)) {
                        Storage &s = Storage::get();
                        char act;
                        int32_t lid, vid;
                        std::tie(act, lid, vid) = p;
                        if (act == 0) {
                            insert_sorted(s.locations[lid].visits, vid, Storage::VisitAtCmp());
                        } else if (act == 1) {
                            erase_sorted(s.locations[lid].visits, vid, Storage::VisitAtCmp());
                        } else if (act == 2) {
                            std::sort(s.locations[lid].visits.begin(), s.locations[lid].visits.end(), Storage::VisitAtCmp());
                        }
                    }
                }
                ::usleep(100);
            }
        }
    };

    struct Visits : Worker<Visit> {
        Visits() {
            //            thread = new std::thread([this] { run(); });
        }
        ~Visits() { delete thread; }

        void update(const Visit &u) { queue.enqueue(u); }

        //        static Visits self[Writer::kMaxWriters];
        //        static Visits &get(int32_t i) { return self[i % Writer::kMaxWriters]; }
        static Visits self[Writer::kMaxWriters];
        static Visits &get(uint32_t i) { return self[i % Writer::kMaxWriters]; }

        void run() {
            //            Storage &s = Storage::get();
            std::cout << "run() " << (size_t)this << std::endl;
            for (;;) {
                //                std::unique_lock<std::mutex> lock(Writer::mutex);
                //                Writer::cond.wait(lock, [] { return Writer::working; });

                while (Writer::working) {
                    Storage &s = Storage::get();
                    Visit v;
                    if (queue.try_dequeue(v)) {
                        using P = Visit::Param;
                        Storage::VisitItem &it = s.visits[v.id];
                        if (v.mask.test(P::kId)) {
                            it.id = v.id;
                        }
                        if (v.mask.test(P::kMark)) {
                            it.mark = v.mark;
                        }
                        if (v.mask.test(P::kUserId)) {
                            if (it.user_id != Entity::kInvalidId) Writer::UserVisits::get(it.user_id).remove(it.user_id, it.id);
                            it.user_id = v.user_id;
                            if (it.user_id != Entity::kInvalidId) Writer::UserVisits::get(it.user_id).add(it.user_id, it.id);
                        }
                        if (v.mask.test(P::kLocationId)) {
                            if (it.location_id != Entity::kInvalidId) Writer::LocationsVisits::get(it.location_id).remove(it.location_id, it.id);
                            it.location_id = v.location_id;
                            if (it.location_id != Entity::kInvalidId) Writer::LocationsVisits::get(it.location_id).add(it.location_id, it.id);
                        }
                        if (v.mask.test(P::kVisitedAt)) {
                            it.visited_at = v.visited_at;
                            if (it.user_id != Entity::kInvalidId) Writer::UserVisits::get(it.user_id).sort(it.user_id, it.id);
                            if (it.location_id != Entity::kInvalidId) Writer::LocationsVisits::get(it.location_id).sort(it.location_id, it.id);
                        }
                    }
                }
                ::usleep(100);
            }
        }
    };

    struct Locations : Worker<Location> {
        Locations() {
            //            thread = new std::thread([this] { run(); });
        }
        ~Locations() { delete thread; }

        void update(Location &&u) { queue.enqueue(u); }

        static Locations self[Writer::kMaxWriters];
        static Locations &get(int32_t i) { return self[i % Writer::kMaxWriters]; }

        void run() {
            //            Storage &s = Storage::get();
            for (;;) {
                //                std::unique_lock<std::mutex> lock(Writer::mutex);
                //                Writer::cond.wait(lock);

                while (Writer::working) {
                    Storage &s = Storage::get();
                    Location l;
                    if (queue.try_dequeue(l)) {
                        using P = Location::Param;
                        Storage::LocationItem &it = s.locations[l.id];
                        if (l.mask.test(P::kId)) {
                            it.id = l.id;
                        }
                        if (l.mask.test(P::kDistance)) {
                            it.distance = l.distance;
                        }

                        if (l.mask.test(P::kCity)) {
                            auto iter = s.cities_map.find(l.city);
                            if (iter == s.cities_map.end()) {
                                Writer::Strings::get().addCity(it.id, l.city);
                            } else {
                                it.city_id = iter->second;
                            }
                        }

                        if (l.mask.test(P::kCountry)) {
                            auto iter = s.countries_map.find(l.country);
                            if (iter == s.countries_map.end()) {
                                Writer::Strings::get().addCountry(it.id, l.country);
                            } else {
                                it.country_id = iter->second;
                            }
                        }

                        if (l.mask.test(P::kPlace)) {
                            auto iter = s.places_map.find(l.place);
                            if (iter == s.places_map.end()) {
                                Writer::Strings::get().addPlace(it.id, l.place);
                            } else {
                                it.place_id = iter->second;
                            }
                        }
                    }
                }
                ::usleep(100);
            }
        }
    };

    struct Users : Worker<User> {
        using Item = User;
        //        mpmc_bounded_queue<Item> queue;
        //        boost::lockfree::spsc_queue<Item, boost::lockfree::capacity<1024 * 1024>> queue;

        std::thread *thread;
        Users() {
            //            thread = new std::thread([this] { run(); });
        }
        ~Users() { delete thread; }

        static Users self[Writer::kMaxWriters];

        static Users &get(int32_t i) { return self[i % Writer::kMaxWriters]; }

        void update(User &&u) { queue.enqueue(u); }

        void run() {
            //            Storage &s = Storage::get();
            for (;;) {
                //                std::unique_lock<std::mutex> lock(Writer::mutex);
                //                Writer::cond.wait(lock);

                while (Writer::working) {
                    Storage &s = Storage::get();
                    Item u;
                    if (queue.try_dequeue(u)) {
                        using P = User::Param;
                        Storage::UserItem &it = s.users[u.id];
                        if (u.mask.test(P::kId)) {
                            it.id = u.id;
                        }
                        if (u.mask.test(P::kGender)) {
                            it.gender = u.gender;
                        }
                        if (u.mask.test(P::kBirthDate)) {
                            it.birth_date = u.birth_date;
                            it.age = s.userAge(it);
                        }
                        if (u.mask.test(P::kEmail)) {
                            it.email = std::move(u.email);
                        }
                        if (u.mask.test(P::kFirstName)) {
                            auto iter = s.first_names_map.find(u.first_name);
                            if (iter == s.first_names_map.end()) {
                                Writer::Strings::get().addFirstName(it.id, u.first_name);
                            } else {
                                it.first_name_id = iter->second;
                            }
                        }

                        if (u.mask.test(P::kLastName)) {
                            auto iter = s.last_names_map.find(u.last_name);
                            if (iter == s.last_names_map.end()) {
                                Writer::Strings::get().addLastName(it.id, u.last_name);
                            } else {
                                it.last_name_id = iter->second;
                            }
                        }
                    }
                }
                ::usleep(100);
            }
        }
    };
};
}

#endif
