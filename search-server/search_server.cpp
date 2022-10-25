#include "search_server.h"
#include "log_duration.h" // матчинг и поиск топ

#include <numeric>
#include <cmath>

using namespace std::literals;

    void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
        if (document_id < 0)
            throw std::invalid_argument("Попытка добавить документ с отрицательным id.");
        if (documents_.count(document_id) > 0)
            throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа.");

        for (std::string_view word : SplitIntoWords(document)) {
            if (!IsValidWord(word))
                throw std::invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.");
        } // проверка

        const std::vector<std::string_view> words = SplitIntoWordsNoStop(document);
        const double step = 1.0 / words.size();
        for (std::string_view word : words) {
            word_to_document_freqs_[std::string(word)][document_id] += step;
            words_freqs_by_documents_[document_id][std::string(word)] += step;
        }
        // example: words = "hello little cat", частота слова cat для этого документа 1/3;(for TF)
        // map<string, map<int, double>> word_to_document_freqs_;
        // map<int, map<string, double>> words_freqs_by_documents_; - по id
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        documents_ids_.insert(document_id);
    }

    std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {

        return FindTopDocuments(raw_query,
                                [status]
                                ([[maybe_unused]]int document_id, DocumentStatus status_predicate, [[maybe_unused]] int rating )
                                { return status == status_predicate; }
                               );
    }

    int SearchServer::GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    SearchServer::DataAfterMatching SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
            std::vector<std::string_view> matched_words;
            matched_words.reserve(query.plus_words.size() + query.minus_words.size());

            for (std::string_view word : query.plus_words) {
                const auto word_position = word_to_document_freqs_.find(word);
                if (word_position == word_to_document_freqs_.cend())
                    continue;

                if (word_position->second.count(document_id))
                    matched_words.push_back(word_position->first);
            }

            for (std::string_view word : query.minus_words) {
                const auto word_position = word_to_document_freqs_.find(word);
                if (word_position == word_to_document_freqs_.cend())
                    continue;

                if (word_position->second.count(document_id)) {
                    matched_words.clear();
                    break;
                }
            }
            return {matched_words, documents_.at(document_id).status};
    }

    SearchServer::DataAfterMatching SearchServer::MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const {
        return MatchDocument(raw_query, document_id);}

    SearchServer::DataAfterMatching SearchServer::MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const {
        using namespace std::execution;

        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;

        for (std::string_view word : SplitIntoWords(raw_query)) {
          const QueryWord query_word = ParseQueryWord(word);
          if (!query_word.is_stop) {
              if (query_word.is_minus) {
                  minus_words.push_back(query_word.data);
              } else {
                  plus_words.push_back(query_word.data);
              }
          }
        }

        const auto ckeck_word = [this, document_id](std::string_view word) {
            const auto pos = word_to_document_freqs_.find(word);
            return pos != word_to_document_freqs_.end() && pos->second.count(document_id);
        };

        //check minus
        if (std::any_of(std::execution::par, minus_words.begin(), minus_words.end(), ckeck_word)) {
            return {std::vector<std::string_view> {}, documents_.at(document_id).status};
        }

        //std::vector<std::string> matched_words(plus_words.size());
        std::vector<std::string_view> matched_words(plus_words.size());

        auto it_end_cpy =
              std::copy_if(std::execution::par, plus_words.begin(), plus_words.end(),
                          matched_words.begin(),
                          [&ckeck_word](std::string_view word) {
                               return ckeck_word(word);
                          });

        matched_words.resize(std::distance(matched_words.begin(), it_end_cpy));

    //  Remove duplicates
        std::sort(std::execution::par, matched_words.begin(), matched_words.end());
        auto last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
        matched_words.erase(last, matched_words.end());

        return {matched_words, documents_.at(document_id).status};
    }
    bool SearchServer::IsValidWord(std::string_view word) {
        // A valid word must not contain special characters
        return std::none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    bool SearchServer::IsStopWord(std::string_view word) const {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
        std::vector<std::string_view> words;
        for (std::string_view word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    } // erase stop words

    int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = std::accumulate(ratings.begin(),ratings.end(), 0);//review
        return rating_sum / static_cast<int>(ratings.size());
    } // считаем средний рейтинг

    SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
        if (!IsValidWord(text))
            throw std::invalid_argument("В словах поискового запроса есть недопустимые символы с кодами от 0 до 31.");
        if (text == "-"s)
            throw std::invalid_argument("Отсутствие текста после символа «минус»: в поисковом запросе.");
        if (text.size() > 1)
            if (text[1] == '-' && text[0] == '-')
                throw std::invalid_argument("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах");
       // проверка
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }


    SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
        Query query;
        for (std::string_view word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.push_back(query_word.data);
                } else {
                    query.plus_words.push_back(query_word.data);
                }
            }
        }

        std::sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(std::unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());

        std::sort(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(std::unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());


        return query;
    } //разбиваем на +- слова

    double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
        return std::log(documents_.size() * 1.0 / word_to_document_freqs_.at(std::string(word)).size());
    } // IDF

    const std::map<std::string_view, double> &SearchServer::GetWordFrequencies(int index) const {
        static const std::map<std::string_view, double> empty_map {};

        if (words_freqs_by_documents_.count(index) > 0)
            return words_freqs_by_documents_.at(index);

        return empty_map;
    }

    void SearchServer::RemoveDocument(int index) {
        auto it_doc_pos = documents_ids_.find(index);

        if (it_doc_pos == documents_ids_.end())
            return;
        // map<int, map<string, double>> words_freqs_by_documents_ -> word : string
        for (const auto& [word, _] : words_freqs_by_documents_.at(index)) {
            // map<string, map<int, double>> word_to_document_freqs_ -> (erase - int)
            word_to_document_freqs_.at(std::string(word)).erase(index);
        }
        documents_ids_.erase(it_doc_pos);
        documents_.erase(index);
        words_freqs_by_documents_.erase(index);
    }

    void SearchServer::RemoveDocument(std::execution::sequenced_policy, int index) {
        return RemoveDocument(index);
    }

    void SearchServer::RemoveDocument(std::execution::parallel_policy, int index) {

        auto it_doc_pos = documents_ids_.find(index);
        if (it_doc_pos == documents_ids_.end())
            return;


        auto& words_freqs = words_freqs_by_documents_.at(index);
        std::vector<const std::string_view*>  p_strs(words_freqs.size());

        std::transform(std::execution::par,
                       words_freqs.begin(), words_freqs.end(),
                       p_strs.begin(),
                       [](const auto& word_freq){//const auto& [word,_] not worked!!!!!????????????????????????S
                            return &(word_freq.first);
                        });

        std::for_each(std::execution::par,
                      p_strs.begin(), p_strs.end(),
                      [&](const auto& ptr_word) {
                            word_to_document_freqs_.at(std::string(*ptr_word)).erase(index);
                        });

        documents_ids_.erase(it_doc_pos);
        documents_.erase(index);
        words_freqs_by_documents_.erase(index);
    }

    std::set<int>::iterator SearchServer::begin() {
        return documents_ids_.begin();
    }

    std::set<int>::const_iterator SearchServer::begin() const {
        return documents_ids_.begin();
    }

    std::set<int>::iterator SearchServer::end() {
        return documents_ids_.end();
    }

    std::set<int>::const_iterator SearchServer::end() const {
        return documents_ids_.end();
    }


