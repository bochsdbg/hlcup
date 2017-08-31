TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++17
QMAKE_LFLAGS += -std=c++17

ragel.output = $$OUT_PWD/${QMAKE_FILE_IN_BASE}
ragel.input = RAGEL_FILES
ragel.commands = ragel -G2 -L ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
ragel.variable_out = HEADERS
ragel.name = RAGEL

QMAKE_EXTRA_COMPILERS += ragel

DEPENDPATH += $$PWD/

INCLUDEPATH += 3rdparty/include

LIBS += $$PWD/3rdparty/lib/libzip.a $$PWD/3rdparty/lib/libz.a -lpthread

RAGEL_FILES = \
    http_parser.hpp.rl \
    json_parser.hpp.rl

SOURCES += main.cpp

HEADERS += \
    HttpServer.hpp \
    Connection.hpp \
    common.hpp \
    Request.hpp \
    HttpParserTest.hpp \
    Storage.hpp \
    date.hpp \
    itoa_branchlut.hpp \
    btree_container.h \
    btree_set.h \
    btree.h \
    Writer.hpp \
    lockfree.hpp \
    concurrentqueue.h

DISTFILES += \
    experiment.txt


