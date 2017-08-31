#include <iomanip>
#include <iostream>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "HttpServer.hpp"

#include <string.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <chrono>

#include "json_parser.hpp"

#include <zip.h>

#include <sys/resource.h>
#include <thread>
#include "Storage.hpp"
#include "itoa_branchlut.hpp"

constexpr const char *hlcup::Connection::ok_header;
constexpr const char *hlcup::Connection::gender_m;
constexpr const char *hlcup::Connection::gender_f;
constexpr const hlcup::IoVec hlcup::Connection::post_ok;
constexpr const hlcup::IoVec hlcup::Connection::err_400_tpl[];
constexpr const hlcup::IoVec hlcup::Connection::err_404_tpl[];
constexpr const char *hlcup::Connection::marks[];

// hlcup::SpinLock hlcup::Storage::countries_lock;
// hlcup::SpinLock hlcup::Storage::places_lock;
// std::array<hlcup::SpinLock, hlcup::Storage::kSpinLockPoolSize> hlcup::Storage::users_visits_lock;
// std::array<hlcup::SpinLock, hlcup::Storage::kSpinLockPoolSize> hlcup::Storage::locations_visits_lock;
// hlcup::SpinLock hlcup::Storage::users_lock;
// hlcup::SpinLock hlcup::Storage::locations_lock;
// hlcup::SpinLock hlcup::Storage::visits_lock;

bool hlcup::Writer::working{false};
std::condition_variable hlcup::Writer::cond;
std::mutex hlcup::Writer::mutex;

hlcup::Writer::Users hlcup::Writer::Users::self[hlcup::Writer::kMaxWriters];
hlcup::Writer::Locations hlcup::Writer::Locations::self[hlcup::Writer::kMaxWriters];
hlcup::Writer::Visits hlcup::Writer::Visits::self[hlcup::Writer::kMaxWriters];
hlcup::Writer::UserVisits hlcup::Writer::UserVisits::self[hlcup::Writer::kMaxWriters];
hlcup::Writer::LocationsVisits hlcup::Writer::LocationsVisits::self[hlcup::Writer::kMaxWriters];

hlcup::Writer::Strings hlcup::Writer::Strings::self;

hlcup::Storage hlcup::Storage::self;
// char *fast_div_write(char *p, uint64_t n, uint32_t d) {
//    int32_t result = ((n * 1000000) / d + 5) / 10;
//    *p++ = (result / 100000) + '0';
//    *p++ = '.';
//    return hlcup::u32toa_branchlut2(result % 100000, p);
//}
//
#include <sstream>
//
std::string div_str(uint64_t n, uint32_t d) {
    if (d == 0) {
        return "0.0";
    }
    std::stringstream buf;
    buf << std::setprecision(5) << ((1.0) * n) / ((1.0) * d);
    return buf.str();
}
//
// void test(uint64_t n, uint32_t d) {
//    char buf[10] = {0};
//    fast_div_write(buf, n, d);
//    std::cout << (n * 1000000 + 1) / d << " " <<  buf << " " << div_str(n, d) << std::endl;
//}

#include <fstream>
#include <map>

// template <typename Parser, typename Item>
// inline size_t bench(const char *p, const char *pe, Parser &parser, Item *item) {
//    while (p != pe) {
//        p = parser.parse(p, pe, &u);
//        if (p == nullptr) break;
//        result += u.id + u.email.size() + u.first_name.size() + u.last_name.size();
//        u_count++;
//    }
//}

int main(int argc, char *argv[]) {
    std::ifstream locs_f("/home/me/prj/highloadcup/dist/../hlcupdocs/data/TRAIN/data/locations_1.json");
    size_t locs_sz = 111103;
    std::ifstream users_f("/home/me/prj/highloadcup/dist/../hlcupdocs/data/TRAIN/data/users_1.json");
    size_t users_sz = 153335;
    std::ifstream visits_f("/home/me/prj/highloadcup/dist/../hlcupdocs/data/TRAIN/data/visits_1.json");
    size_t visits_sz = 801327;

    std::string locs(locs_sz, 0), visits(visits_sz, 0), users(users_sz, 0);

    locs_f.read(&locs[0], locs_sz);
    users_f.read(&users[0], users_sz);
    visits_f.read(&visits[0], visits_sz);

    size_t result = 0;
    size_t u_count = 0, l_count = 0, v_count = 0;

    hlcup::UserParser up;
    hlcup::VisitParser vp;
    hlcup::LocationParser lp;

    const char *uptr = users.data() + users.find('[') + 1;
    const char *uptr_end = users.data() + users.size();
    const char *vptr = visits.data() + visits.find('[') + 1;
    const char *vptr_end = visits.data() + visits.size();
    const char *lptr = locs.data() + locs.find('[') + 1;
    const char *lptr_end = locs.data() + locs.size();

    int count = 1024 * 4;
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    for (int k = 0; k < count; k++) {
        {
            hlcup::User u;
            up.reset();
            const char *p = uptr;
            const char *pe = uptr_end;
            while (p != pe) {
                p = up.parse(p, pe, &u);
                if (p == nullptr) break;
                //                result += u.id + u.email.size() + u.first_name.size() + u.last_name.size();
                //                u_count++;
            }
        }

        {
            hlcup::Location l;
            lp.reset();
            const char *p = lptr;
            const char *pe = lptr_end;
            while (p != pe) {
                p = lp.parse(p, pe, &l);
                if (p == nullptr) break;
                //                result += l.id + l.city.size() + l.country.size() + l.place.size();
                //                l_count++;
            }
        }

        {
            hlcup::Visit v;
            vp.reset();
            const char *p = vptr;
            const char *pe = vptr_end;
            while (p != pe) {
                p = vp.parse(p, pe, &v);
                if (p == nullptr) break;
                //                result += v.id + v.mark;
                //                v_count++;
            }
        }
    }

    timespec ts2;
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    double dt = 1.0 * (ts2.tv_sec - ts.tv_sec) + 1e-9 * (ts2.tv_nsec - ts.tv_nsec);
    std::cout << std::setprecision(15) << dt << std::endl;
    size_t data_size = lptr_end - lptr + vptr_end - vptr + uptr_end - uptr;
    std::cout << std::setprecision(15) << (1.0 * count * data_size) / dt / (1024.0 * 1024.0) << std::endl;

    std::cout << std::endl;
    std::cout << u_count << " " << l_count << " " << v_count << std::endl;
    std::cout << result << std::endl;
    return 0;
    //    std::cout << sizeof(hlcup::Storage::VisitItem) << " " << sizeof(hlcup::Storage::VisitItem) * hlcup::Storage::kVisitsMax << std::endl;
    //    std::cout << sizeof(hlcup::Storage::UserItem) << " " << sizeof(hlcup::Storage::UserItem) * hlcup::Storage::kUsersMax << std::endl;
    //    std::cout << sizeof(hlcup::Storage::LocationItem) << " " << sizeof(hlcup::Storage::LocationItem) * hlcup::Storage::kLocationsMax << std::endl;
    //    return 0;
    //
    //    for (uint64_t n = 0; n < 10000; n++) {
    //        for (uint32_t d = 0; d < 1000; d++) {
    //            char buf[100] = {'0', '.', '0', '0', '0', '0', '0', 0};
    //            char *buf_end = hlcup::fast_div_write(buf, n, d);
    //            std::string s = div_str(n, d);
    //            if (buf_end - buf > 7 || atof(buf) != atof(s.data())) {
    //                std::cerr << n << " " << d << " " << buf << " " << s << std::endl;
    //                return 1;
    //            }
    //
    //        }
    //    }
    //    std::cerr << "ok" << std::endl;
    //    return 0;
    //    test(10, 3);
    //    test(10, 4);
    //    test(10, 6);
    //    test(10, 10);
    //    return 0;

    //    char buf[15] = {0};
    //    std::cout << hlcup::i32toa_branchlut2(112, buf) << std::endl;

    //    std::cout << buf << std::endl;

    //    return 0;

    auto &s = hlcup::Storage().get();

    //    thread_local int i = 0;
    //    std::thread th([&]() { i++; });
    //    th.join();
    //    std::cout << i << std::endl;
    //    return 0;

    s.loadTimestamp("/tmp/data/options.txt");
    std::cerr << "Current timestamp: " << s.current_time << std::endl;

    int32_t u, l, v;
    std::cerr << "Staring server..." << std::endl;
    std::tie(u, l, v) = s.loadFromFile("/tmp/data/data.zip");
    std::cerr << "Loaded: " << u << " users, " << l << " locations, " << v << " visits" << std::endl;

    //    std::map<std::string, uint32_t> first_names_stat, last_names_stat, places_stat, countries_stat;

    //    for (auto &e: s.users) {
    //        first_names_stat[e.first_name]++;
    //        last_names_stat[e.last_name]++;
    //    }
    //
    //    for (auto &e: s.locations) {
    //        places_stat[e.place]++;
    //        countries_stat[e.country]++;
    //    }

    //    int i = 0;
    //    for (auto &it: places_stat) {
    //        std::cout << ++i << " " << it.first << " " << it.second << std::endl;
    //    }
    //    i=0;
    //    for (auto &it: countries_stat) {
    //        std::cout << ++i << " " << it.first << " " << it.second << std::endl;
    //    }
    //    i=0;
    //    for (auto &it: first_names_stat) {
    //        std::cout << ++i << " " << it.first << " " << it.second << std::endl;
    //    }
    //    i=0;
    //    for (auto &it: last_names_stat) {
    //        std::cout << ++i << " " <<  it.first << " " << it.second << std::endl;
    //    }
    //
    // return 0;
    rlimit rlim{0x7FFFFFFF, 0x7FFFFFFF};
    if (0 < setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        perror("setrlimit(RLIMIT_MEMLOCK)");
    }
    int err = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (err < 0) {
        perror("mlockall()");
    }
    rlimit rlim2{1024 * 1024, 1024 * 1024};
    setrlimit(RLIMIT_STACK, &rlim2);
    rlimit rlim3{8192, 8192};
    setrlimit(RLIMIT_NOFILE, &rlim3);

    //    std::thread warmup_th([] {
    //        auto &s = hlcup::Storage().get();
    //        volatile size_t id_sum = 0;
    //        for (size_t i = 0; i < s.users.size(); i++) {
    //            id_sum += s.users[i].id + s.users[i].birth_date;
    //        }
    //        for (size_t i = 0; i < s.locations.size(); i++) {
    //            id_sum += s.locations[i].id + s.locations[i].distance;
    //        }
    //        for (size_t i = 0; i < s.visits.size(); i++) {
    //            id_sum += s.visits[i].id + s.visits[i].visited_at;
    //        }
    //        for (size_t i = 0; i < s.users_visits.size(); i++) {
    //            for (int32_t id : s.users_visits[i]) {
    //                id_sum += id;
    //            }
    //        }
    //        for (size_t i = 0; i < s.locations_visits.size(); i++) {
    //            for (int32_t id : s.locations_visits[i]) {
    //                id_sum += id;
    //            }
    //        }

    //        usleep(200000);
    //    });

    //    hlcup::HttpParser p;

    //    std::string query1 = "GET /users/123?fromDate=3643534534&toDate=123 HTTP/1.1\r\n";

    //    std::string data = "";

    //    for (int i = 0; i < 1; i++) {
    //        data += query1;
    //    }

    //    const char *data_ptr = data.data();
    //    const char *data_end = data.data() + data.size();

    //    int count = 1024 * 1024 * 64;
    //    timespec ts;
    //    clock_gettime(CLOCK_MONOTONIC, &ts);

    //    for (int k = 0; k < count; k++) {
    //        data_ptr = data.data();
    //        p.reset();
    //        while (data_ptr != data_end) {
    //            data_ptr = p.parse(data_ptr, data_end);
    //            //        std::cout << p.r.action << " " << p.r.entity_id << " " << p.r.from_date << " " << p.r.to_date << " " << p.r.mask << std::endl;
    //            //            p.reset();
    //            p.reset();
    //        }
    //    }

    //    timespec ts2;
    //    clock_gettime(CLOCK_MONOTONIC, &ts2);
    //    double dt = 1.0 * (ts2.tv_sec - ts.tv_sec) + 1e-9 * (ts2.tv_nsec - ts.tv_nsec);
    //    std::cout << std::setprecision(15) << dt << std::endl;
    //    std::cout << std::setprecision(15) << (1.0 * count * data.size()) / dt / (1024.0 * 1024.0) << std::endl;

    //    return 0;

    int port = 80;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    //    hlcup::HttpServer serv;
    //    serv.startListener(port);
    //    serv.run();

    std::vector<std::thread> ths;
    for (int i = 0; i < 10; i++) {
        ths.emplace_back([&] {
            hlcup::HttpServer serv;
            serv.startListener(port);
            serv.run();
        });
    }

    //    std::thread thh(hlcup::Writer::Visits::self[0]);
    //    ths.emplace_back(hlcup::Writer::Visits());  // hlcup::Writer::Visits::self[0]);

    for (auto &th : ths) {
        if (th.joinable()) {
            th.join();
        }
    }

    return 0;
}
