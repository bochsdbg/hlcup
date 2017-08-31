#ifndef HLCUP_REQUEST_HPP__
#define HLCUP_REQUEST_HPP__

#include <bitset>

#include "common.hpp"

namespace hlcup {

struct Request {
    std::bitset<8> mask;
    typedef int32_t time_t;

    enum Param { kFromDate = 0, kToDate, kFromAge, kToAge, kCountry, kToDistance, kGender };
    enum Action {
        kNotFound = 0,
        kBadRequest,
        kGetIndex,
        kGetUser,
        kGetLocation,
        kGetVisit,
        kGetUserVisits,
        kGetLocationAvg,
        kPostUser,
        kPostUserNew,
        kPostLocation,
        kPostLocationNew,
        kPostVisit,
        kPostVisitNew
    };

    static const Action kGetLast = kGetLocationAvg;

    Action action = Action::kNotFound;

    int32_t entity_id = Entity::kInvalidId;

    time_t from_date = 0, to_date = 0;
    int32_t from_age, to_age;
    std::string country;
    int32_t to_distance;
    Gender gender;

    User u;
    Location l;
    Visit v;

    void reset() {
        mask.reset();
        action = Action::kNotFound;
        entity_id = Entity::kInvalidId;
    }

    bool isSet(Param p) { return mask.test(static_cast<size_t>(p)); }
};
}

#endif
