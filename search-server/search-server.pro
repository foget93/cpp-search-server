TEMPLATE = app
CONFIG += console c++20
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        document.cpp \
        main.cpp \
        #old_main.cpp \
        process_queries.cpp \
        read_input_functions.cpp \
        remove_duplicates.cpp \
        request_queue.cpp \
        search_server.cpp \
        string_processing.cpp \
        test_example_functions.cpp \
        unit_tests.cpp

HEADERS += \
    concurrent_map.h \
    document.h \
    log_duration.h \
    paginator.h \
    process_queries.h \
    read_input_functions.h \
    remove_duplicates.h \
    request_queue.h \
    search_server.h \
    string_processing.h \
    test_example_functions.h \
    unit_tests.h

LIBS += -ltbb \    #libtbb-dev — вспомогательная библиотека Thread Building Blocks от Intel для реализации параллельности.
        -lpthread
