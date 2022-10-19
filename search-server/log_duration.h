#pragma once

#include <chrono>
#include <iostream>
#include <string_view>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)

/**
 * ÐœÐ°ÐºÑ€Ð¾Ñ Ð·Ð°Ð¼ÐµÑ€ÑÐµÑ‚ Ð²Ñ€ÐµÐ¼Ñ, Ð¿Ñ€Ð¾ÑˆÐµÐ´ÑˆÐµÐµ Ñ Ð¼Ð¾Ð¼ÐµÐ½Ñ‚Ð° ÑÐ²Ð¾ÐµÐ³Ð¾ Ð²Ñ‹Ð·Ð¾Ð²Ð°
 * Ð´Ð¾ ÐºÐ¾Ð½Ñ†Ð° Ñ‚ÐµÐºÑƒÑ‰ÐµÐ³Ð¾ Ð±Ð»Ð¾ÐºÐ°, Ð¸ Ð²Ñ‹Ð²Ð¾Ð´Ð¸Ñ‚ Ð² Ð¿Ð¾Ñ‚Ð¾Ðº std::cerr.
 *
 * ÐŸÑ€Ð¸Ð¼ÐµÑ€ Ð¸ÑÐ¿Ð¾Ð»ÑŒÐ·Ð¾Ð²Ð°Ð½Ð¸Ñ:
 *
 *  void Task1() {
 *      LOG_DURATION("Task 1"s); // Ð’Ñ‹Ð²ÐµÐ´ÐµÑ‚ Ð² cerr Ð²Ñ€ÐµÐ¼Ñ Ñ€Ð°Ð±Ð¾Ñ‚Ñ‹ Ñ„ÑƒÐ½ÐºÑ†Ð¸Ð¸ Task1
 *      ...
 *  }
 *
 *  void Task2() {
 *      LOG_DURATION("Task 2"s); // Ð’Ñ‹Ð²ÐµÐ´ÐµÑ‚ Ð² cerr Ð²Ñ€ÐµÐ¼Ñ Ñ€Ð°Ð±Ð¾Ñ‚Ñ‹ Ñ„ÑƒÐ½ÐºÑ†Ð¸Ð¸ Task2
 *      ...
 *  }
 *
 *  int main() {
 *      LOG_DURATION("main"s);  // Ð’Ñ‹Ð²ÐµÐ´ÐµÑ‚ Ð² cerr Ð²Ñ€ÐµÐ¼Ñ Ñ€Ð°Ð±Ð¾Ñ‚Ñ‹ Ñ„ÑƒÐ½ÐºÑ†Ð¸Ð¸ main
 *      Task1();
 *      Task2();
 *  }
 */
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

/**
 *
 *  int main() {
 *      // Ð’Ñ‹Ð²ÐµÐ´ÐµÑ‚ Ð²Ñ€ÐµÐ¼Ñ Ñ€Ð°Ð±Ð¾Ñ‚Ñ‹ main Ð² Ð¿Ð¾Ñ‚Ð¾Ðº std::cout
 *      LOG_DURATION("main"s, std::cout);
 *      ...
 *  }
 */
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;

    LogDuration(std::string_view id, std::ostream& dst_stream = std::cerr)
        : id_(id)
        , dst_stream_(dst_stream) {
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        dst_stream_ << id_ << ": "sv << duration_cast<milliseconds>(dur).count() << " ms"sv << std::endl;
    }

private:
    const std::string id_;
    const Clock::time_point start_time_ = Clock::now();
    std::ostream& dst_stream_;
};


/*#pragma once
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
*/
