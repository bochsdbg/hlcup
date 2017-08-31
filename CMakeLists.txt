cmake_minimum_required(VERSION 3.8)
project(server)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_VERBOSE_MAKEFILE 1)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
find_package(RAGEL 7.0 REQUIRED)
RAGEL_TARGET(json_parser json_parser.hpp.rl ${CMAKE_CURRENT_BINARY_DIR}/json_parser.hpp)
RAGEL_TARGET(http_parser http_parser.hpp.rl ${CMAKE_CURRENT_BINARY_DIR}/http_parser.hpp)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR})



set(SOURCE_FILES
        3rdparty/include/zconf.h
        3rdparty/include/zip.h
        3rdparty/include/zlib.h
        3rdparty/lib/libzip/include/zipconf.h
        btree.h
        btree_container.h
        btree_set.h
        common.hpp
        Connection.hpp
        date.hpp
        experiment.txt
        HttpParserTest.hpp
        HttpServer.hpp
        itoa_branchlut.hpp
        main.cpp
        http_parser
        json_parser
        Request.hpp
        Storage.hpp)

link_libraries(${CMAKE_SOURCE_DIR}/3rdparty/lib/libzip.a ${CMAKE_SOURCE_DIR}/3rdparty/lib/libz.a pthread)
add_executable(server ${SOURCE_FILES})
