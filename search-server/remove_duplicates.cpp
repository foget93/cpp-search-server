#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    std::map<std::set<std::string>, int> words_ids;
    std::vector<int> remove_indexs;

    for (const int index : search_server) {
        //const std::map<std::string, double>
        const auto &word_frequencies = search_server.GetWordFrequencies(index);
        std::set<std::string> words;
        for (auto map_it = word_frequencies.begin(); map_it != word_frequencies.end(); ++map_it)
            words.insert(map_it->first);

        if (words_ids.count(words) > 0)
            remove_indexs.push_back(index); // добавляем дубликаты на удаление
        else
            words_ids.insert({words, index}); // уникальные слова + id
    }

    for (int index : remove_indexs) {
        search_server.RemoveDocument(index);
        std::cout << "Found duplicate document id " << index << std::endl;
    }
}
