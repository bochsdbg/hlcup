#ifndef HLCUP_STORAGE_HPP__
#define HLCUP_STORAGE_HPP__

#include <bitset>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <zip.h>

#include "Request.hpp"
#include "date.hpp"
#include "json_parser.hpp"

#include "date.hpp"

#include <set>
#include <unordered_map>
#include <unordered_set>

#include <boost/container/flat_set.hpp>
//#include <set>

namespace hlcup {

template <typename T, typename Pred>
typename std::vector<T>::iterator insert_sorted(std::vector<T> &vec, T const &item, Pred pred) {
    return vec.insert(std::upper_bound(vec.begin(), vec.end(), item, pred), item);
}

template <typename T, typename Pred>
void erase_sorted(std::vector<T> &vec, T const &item, Pred pred) {
    auto iter = std::lower_bound(vec.begin(), vec.end(), item, pred);
    if (iter != vec.end() && *iter == item) {
        vec.erase(iter);
    }
}

struct Storage {
    //    static const size_t kSpinLockPoolSize = 0xFFFF;

    //    static SpinLock countries_lock;
    //    static SpinLock places_lock;
    //    static std::array<hlcup::SpinLock, hlcup::Storage::kSpinLockPoolSize> users_visits_lock;
    //    static std::array<hlcup::SpinLock, hlcup::Storage::kSpinLockPoolSize> locations_visits_lock;
    //    static SpinLock users_lock;
    //    static SpinLock locations_lock;
    //    static SpinLock visits_lock;

    struct UserItem {
        int32_t id = Entity::kInvalidId;
        time_t birth_date;
        Gender gender : 1;
        unsigned char age : 7;
        int32_t first_name_id = Entity::kInvalidId, last_name_id = Entity::kInvalidId;
        std::vector<int32_t> visits;
        std::string email;
    };

    struct LocationItem {
        int32_t id = Entity::kInvalidId;
        int32_t distance;
        int32_t place_id = Entity::kInvalidId, country_id = Entity::kInvalidId, city_id = Entity::kInvalidId;
        std::vector<int32_t> visits;
    };

    struct VisitItem {
        unsigned char mark : 3;
        int32_t id : 29;
        int32_t location_id = Entity::kInvalidId;
        int32_t user_id = Entity::kInvalidId;
        int32_t visited_at = 0;

        VisitItem() : mark(0), id(Entity::kInvalidId) {}
        VisitItem(const Visit &v) : mark(v.mark), id(v.id), location_id(v.location_id), user_id(v.user_id), visited_at(v.visited_at) {}
    };

    static const size_t kUsersMax = 10 * 1024 * 1024;
    static const size_t kLocationsMax = 10 * 1024 * 1024;
    static const size_t kVisitsMax = 40 * 1024 * 1024;

    //    std::vector<UserItem> users;
    //    std::vector<LocationItem> locations;
    //    std::vector<VisitItem> visits;
    UserItem *users = nullptr;
    LocationItem *locations = nullptr;
    VisitItem *visits = nullptr;

    //    std::vector<unsigned char> users_ages;
    //    std::vector<std::set<int32_t, UsersVisitsCmp>> users_visits;
    //    std::vector<std::unordered_set<int32_t>> locations_visits;

    std::vector<std::string> places, countries, cities, first_names, last_names;
    std::unordered_map<std::string, int32_t> places_map, countries_map, cities_map, first_names_map, last_names_map;

    Storage() : users(new UserItem[kUsersMax]), locations(new LocationItem[kLocationsMax]), visits(new VisitItem[kVisitsMax]) {}
    ~Storage() {
        delete[] users;
        delete[] locations;
        delete[] visits;
    }
    struct VisitAtCmp {
        inline bool operator()(int32_t a, int32_t b) const {
            int32_t t1 = Storage::get().visits[a].visited_at;
            int32_t t2 = Storage::get().visits[b].visited_at;
            if (a == b) return false;
            return t1 < t2;
        }
    };

    time_t current_time;

    //    static const int32_t kSecondsInYear = 31556952;  // 365.2425 days
    static const int32_t kSecondsInYear = 31557600;  // 365.25 days

    static Storage self;

    static Storage &get() { return self; }

    bool loadTimestamp(const std::string &file_name) {
        std::ifstream fs(file_name);
        if (fs.good()) {
            fs >> current_time;
            return true;
        }
        return false;
    }

    std::tuple<uint32_t, uint32_t, uint32_t> loadFromFile(const std::string &file_name) {
        int err;
        uint32_t users_count = 0, locations_count = 0, visits_count = 0;
        zip_t *zh = zip_open(file_name.c_str(), ZIP_RDONLY, &err);
        if (zh == nullptr) {
            char buf[255];
            zip_error_to_str(buf, sizeof(buf), err, errno);
            std::cerr << "open(): " << buf << std::endl;
            return std::make_tuple(0, 0, 0);
        }

        for (zip_int64_t i = 0, len = zip_get_num_entries(zh, 0); i < len; i++) {
            zip_stat_t stat;
            zip_stat_index(zh, i, 0, &stat);
            zip_file *zf = zip_fopen_index(zh, i, 0);
            std::string data(static_cast<size_t>(stat.size), 0);
            zip_fread(zf, &data[0], data.size());

            if (stat.name[0] == 'u') {
                users_count += parse<UserParser, User>(data);
            } else if (stat.name[0] == 'l') {
                locations_count += parse<LocationParser, Location>(data);
            } else if (stat.name[0] == 'v') {
                visits_count += parse<VisitParser, Visit>(data);
            } else if (stat.name[0] == 'o') {
                current_time = std::atoi(data.data());
            }
        }

        zip_close(zh);

        //        for (VisitItem &v : visits) {
        //            if (v.id == Entity::kInvalidId) continue;
        //            insert_sorted(users[v.user_id].visits, v.id, VisitAtCmp());
        //            insert_sorted(locations[v.location_id].visits, v.id, VisitAtCmp());
        //            //            updateVisitUser(v.id, Entity::kInvalidId, v.user_id);
        //            //            updateVisitLocation(v.id, Entity::kInvalidId, v.location_id);
        //        }

        return std::make_tuple(users_count, locations_count, visits_count);
    }

    static inline int32_t find_or_create_in_dict(std::string &&s, std::vector<std::string> &vec, std::unordered_map<std::string, int32_t> &map) {
        auto iter = map.find(s);
        if (iter == map.end()) {
            int32_t id = vec.size();
            map[s] = id;
            vec.emplace_back(s);
            return id;
        } else {
            return iter->second;
        }
    }
    void insert_no_check(Location &&e) {
        int32_t id = e.id;
        LocationItem &it = locations[id];
        it.id = id;
        it.place_id = find_or_create_in_dict(std::move(e.place), places, places_map);
        it.country_id = find_or_create_in_dict(std::move(e.country), countries, countries_map);
        it.city_id = find_or_create_in_dict(std::move(e.city), cities, cities_map);
        it.distance = e.distance;
    }

    void insert_no_check(User &&e) {
        int32_t id = e.id;
        UserItem &it = users[id];
        it.id = id;
        it.first_name_id = find_or_create_in_dict(std::move(e.first_name), first_names, first_names_map);
        it.last_name_id = find_or_create_in_dict(std::move(e.last_name), last_names, last_names_map);
        it.birth_date = e.birth_date;
        it.email = std::move(e.email);
        it.gender = e.gender;
        it.age = userAge(it);
    }

    void insert_no_check(Visit &&e) {
        visits[e.id] = e;
        insert_sorted(users[e.user_id].visits, e.id, VisitAtCmp());
        insert_sorted(locations[e.location_id].visits, e.id, VisitAtCmp());
    }

    //    void insert_no_check(const Visit &e) { visits[e.id] = e; }

    //    void updateVisitUser(int32_t vid, int32_t old_uid, int32_t new_uid) {
    //        if (old_uid != Entity::kInvalidId) {
    //            users_visits_lock[old_uid % kSpinLockPoolSize].lock();
    //            users_visits[old_uid].erase(vid);
    //            //            erase_sorted(users_visits[old_uid], vid, UsersVisitsCmp());
    //            users_visits_lock[old_uid % kSpinLockPoolSize].unlock();
    //        }
    //        if (new_uid != Entity::kInvalidId) {
    //            users_visits_lock[new_uid % kSpinLockPoolSize].lock();
    //            users_visits[new_uid].insert(vid);
    //            //            insert_sorted(users_visits[new_uid], vid, UsersVisitsCmp());
    //            users_visits_lock[new_uid % kSpinLockPoolSize].unlock();
    //        }
    //    }
    //    void updateVisitLocation(int32_t vid, int32_t old_lid, int32_t new_lid) {
    //        if (old_lid != Entity::kInvalidId) {
    //            locations_visits_lock[old_lid % kSpinLockPoolSize].lock();
    //            locations_visits[old_lid].erase(vid);
    //            locations_visits_lock[old_lid % kSpinLockPoolSize].unlock();
    //        }
    //        if (new_lid != Entity::kInvalidId) {
    //            locations_visits_lock[new_lid % kSpinLockPoolSize].lock();
    //            locations_visits[new_lid].insert(vid);
    //            locations_visits_lock[new_lid % kSpinLockPoolSize].unlock();
    //        }
    //    }

    inline unsigned char userAge(const UserItem &it) const {
        //        return (((unsigned char)it.gender) << 7) | (unsigned char)((static_cast<int64_t>(current_time) - it.birth_date) / Storage::kSecondsInYear);
        return (unsigned char)((static_cast<int64_t>(current_time) - it.birth_date) / Storage::kSecondsInYear);
    }

    bool userExists(int32_t id) const { return static_cast<unsigned>(id) < kUsersMax && users[id].id != Entity::kInvalidId; }
    bool locationExists(int32_t id) const { return static_cast<unsigned>(id) < kLocationsMax && locations[id].id != Entity::kInvalidId; }
    bool visitExists(int32_t id) const { return static_cast<unsigned>(id) < kVisitsMax && visits[id].id != Entity::kInvalidId; }

private:
    template <class ParserType, class EntityType>
    uint32_t parse(const std::string &data) {
        ParserType p;
        p.reset();
        EntityType e;
        uint32_t count = 0;
        const char *ptr = data.data() + data.find('[') + 1;
        const char *pe = data.data() + data.size();
        while (ptr != pe) {
            ptr = p.parse(ptr, pe, &e);

            if (ptr == nullptr) {
                break;
            }
            p.reset();
            insert_no_check(std::move(e));
            count++;
        }
        return count;
    }
};
}

#endif  // STORAGE_H
