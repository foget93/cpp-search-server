#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

//std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWords(std::string_view text); // string_view

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;

    for (std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.emplace(str);
        }
    }
    return non_empty_strings;
}
