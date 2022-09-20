TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        document.cpp \
        main.cpp \
        read_input_functions.cpp \
        remove_duplicates.cpp \
        request_queue.cpp \
        search_server.cpp \
        string_processing.cpp \
        test_example_functions.cpp \
        unit_tests.cpp

HEADERS += \
    document.h \
    log_duration.h \
    paginator.h \
    read_input_functions.h \
    remove_duplicates.h \
    request_queue.h \
    search_server.h \
    string_processing.h \
    test_example_functions.h \
    unit_tests.h
