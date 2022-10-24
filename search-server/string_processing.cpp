#include "string_processing.h"

/*std::vector<std::string_view> SplitIntoWords(std::string_view text) {
    std::vector<std::string_view> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    } //last word
    return words;
} */

//string to vector<string> ->" "
//transform_reduce....))) ' '+char

std::vector<std::string_view> SplitIntoWords(std::string_view str) {
    std::vector<std::string_view> result;
    //int64_t pos = str.find_first_not_of(" ");

    const int64_t pos_end = str.npos;
    str.remove_prefix(std::min(str.size(), str.find_first_not_of(" ")));

    while (/*pos != pos_end*/!str.empty()) {
        int64_t space = str.find(' ');
        result.push_back(space == pos_end ? str : str.substr(0, space));
        str.remove_prefix(std::min(str.find_first_not_of(" ", space), str.size()));
    }

    return result;
}
//std::vector<std::string_view> SplitIntoWords(std::string_view text) {
//    std::vector<std::string_view> words;
//    int word_begin{0u};

//    while (word_begin <= text.length()) {
//        int word_end = text.find(' ', word_begin);

//        words.push_back(text.substr(word_begin, word_end - word_begin));
//        word_begin = word_end == std::string_view::npos ? word_end : word_end + 1;
//    }

//    return words;
//}
