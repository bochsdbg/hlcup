#ifndef HLCUP_COMMON_HPP__
#define HLCUP_COMMON_HPP__

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <sys/uio.h>

#include <atomic>

#define CALL_RETRY(retvar, expression) \
    do {                               \
        retvar = (expression);         \
    } while (retvar == -1 && errno == EINTR);

namespace hlcup {

enum class Gender : bool { kMale = 0, kFemale };
using time_t = int32_t;

template <typename T, size_t sz>
inline constexpr size_t get_size(T (&)[sz]) {
    return sz;
}

struct SpinLock {
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) {
            ;
        }
    }

    bool try_lock() { return locked.test_and_set(std::memory_order_acquire); }

    void unlock() { locked.clear(std::memory_order_release); }

private:
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
};

constexpr std::size_t length(const char *str) { return (!str || !*str) ? 0 : (1 + length(str + 1)); }

struct IoVec : iovec {
    constexpr inline IoVec(const char *s) : iovec{const_cast<char *>(s), length(s)} {}
    constexpr inline IoVec(std::nullptr_t) : iovec{nullptr, 0} {}
    inline IoVec(std::string &s) : iovec{const_cast<char *>(s.data()), s.size()} {}
};

struct Entity {
    static const int32_t kInvalidId = -1;
    int32_t id = kInvalidId;
};

struct User : Entity {
    enum Param { kId = 0, kBirthDate, kGender, kEmail, kFirstName, kLastName, kLast };

    std::bitset<static_cast<size_t>(Param::kLast)> mask;

    time_t birth_date;
    Gender gender;
    std::string email, first_name, last_name;
};

struct Location : Entity {
    enum Param { kId = 0, kDistance, kPlace, kCountry, kCity, kLast };

    std::bitset<static_cast<size_t>(Param::kLast)> mask;

    int32_t distance;
    std::string place, country, city;
};

struct Visit : Entity {
    enum Param { kId = 0, kLocationId, kUserId, kVisitedAt, kMark, kLast };

    std::bitset<static_cast<size_t>(Param::kLast)> mask;

    int32_t location_id = kInvalidId, user_id = kInvalidId;
    time_t visited_at = 0;
    unsigned char mark = 0;
};
}

#endif
