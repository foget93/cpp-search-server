#pragma once

#include <chrono>
#include <iostream>

//using namespace std::literals;

class LogDuration {
public:
    // заменим имя типа std::chrono::steady_clock
    // с помощью using для удобства
    using Clock = std::chrono::steady_clock;

    LogDuration(std::string name, std::ostream& out_stream = std::cerr)
        : name_(name), out_stream_(out_stream) {}

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        out_stream_ << name_ << ": "s << duration_cast<microseconds>(dur).count() << " mcs"s << std::endl;
    }

private:
    const Clock::time_point start_time_ = Clock::now();
    std::string name_;
    std::ostream& out_stream_;
};

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION_STREAM(x, out_stream) LogDuration UNIQUE_VAR_NAME_PROFILE(x, out_stream)