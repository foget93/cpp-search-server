#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

#include <functional>
#include <execution>

#include "document.h"
#include "string_processing.h"

#include "concurrent_map.h"
//using

const size_t MAX_RESULT_DOCUMENT_COUNT {5};

class SearchServer {
public:

    using DataAfterMatching = std::tuple<std::vector<std::string_view>, DocumentStatus>;

    SearchServer() = default; // старые тесты без стоп слов

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {} // Invoke delegating constructor from string container

    explicit SearchServer(std::string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {}

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
//new
    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                           std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy,
                                           std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    int GetDocumentCount() const;

    DataAfterMatching MatchDocument(std::string_view raw_query, int document_id) const;
    DataAfterMatching MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;
    DataAfterMatching MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;

    //int GetDocumentId(int index) const; //- отказ 5 спринт
    const std::map<std::string_view, double> &GetWordFrequencies(int index) const;

    void RemoveDocument(int index);
    void RemoveDocument(std::execution::sequenced_policy, int index);
    void RemoveDocument(std::execution::parallel_policy, int index);

    std::set<int>::iterator begin();
    std::set<int>::const_iterator begin() const;
    std::set<int>::iterator end();
    std::set<int>::const_iterator end() const;

private:
    struct DocumentData {
        int rating {0};
        DocumentStatus status {DocumentStatus::ACTUAL};
    };

    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_; // word -> id документов с word и их idf*tf
    std::map<int, std::map<std::string_view, double>> words_freqs_by_documents_;
    std::map<int, DocumentData> documents_;  // id + средний рейтинг + статус
    std::set<int> documents_ids_;

    //=======================

    static bool IsValidWord(std::string_view word);

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const; // erase stop words

    static int ComputeAverageRating(const std::vector<int>& ratings); // считаем средний рейтинг

    struct QueryWord {
        std::string_view data;
        bool is_minus{false};
        bool is_stop{false};
    };

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const ; //разбиваем на +- слова

    QueryWord ParseQueryWord(std::string_view text) const; // mb bool

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy,
                                           const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

};

// ======================================== реализации шаблонов ===========================================

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (std::any_of(stop_words_.cbegin(), stop_words_.cend(),
              [](const std::string& word){ return !IsValidWord(word); } )) {
        throw std::invalid_argument("Стоп-слово содержит недопустимые символы!");
    } // https://en.cppreference.com/w/cpp/algorithm/all_any_none_of
}

template <typename ExecutionPolicy,typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                                     std::string_view raw_query, DocumentPredicate document_predicate) const {

    const Query query = ParseQuery(raw_query);// исключения бросаются в ParseQueryWord
    std::vector<Document> result = FindAllDocuments(policy, query, document_predicate);

    const double EPSILON = 1e-6;
    std::sort(
            policy,
            result.begin(), result.end(),
            [EPSILON](const Document& lhs, const Document& rhs) {
                if(std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
         }
    );
    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return result;
}
// ищем все доки по плюс минус словам запроса, и предикату или DocumentStatus, снизу перегрузки FindTopDocuments.

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy,
                                       std::string_view raw_query, DocumentStatus status) const {
        return FindTopDocuments(
                                policy,
                                raw_query,
                                [status]
                                ([[maybe_unused]]int document_id, DocumentStatus status_predicate, [[maybe_unused]] int rating )
                                { return status == status_predicate; }
                               );

}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy,
                                       const Query& query, DocumentPredicate document_predicate) const {
    if constexpr
        (std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>) {

        std::map<int, double> document_to_relevance; // key: id, value: relevance

        for (std::string_view word : query.plus_words) {
            //const auto word_pos = word_to_document_freqs_.find(word);
            if (word_to_document_freqs_.count(word) == 0) {
            //if (word_pos == word_to_document_freqs_.end())
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);// compute IDF
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq; // idf*TF
                }
            }
        }

        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
                document_to_relevance.erase(document_id); // delete for minus word
            }
        }

        std::vector<Document> matched_documents;
        matched_documents.reserve(document_to_relevance.size());

        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating}); //пушим id, relevance, rating найденных
        }
        return matched_documents;

    } else { // std::execution::parallel_policy
        size_t available_cores = std::thread::hardware_concurrency() * 10u;
        ConcurrentMap<int, double> concurent_document_to_relevance(available_cores);

        auto insert_freq_func = [this, &document_predicate, &concurent_document_to_relevance]
                                (std::string_view word) {
            const auto word_pos = word_to_document_freqs_.find(word);
            if (word_pos != word_to_document_freqs_.end()) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        concurent_document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq; // idf*TF ConcurrentMap
                    }
                }
            }
        };

        ForEach(policy, query.plus_words, insert_freq_func); // in concurrent_map.h

        auto erase_minus_func = [this, &concurent_document_to_relevance]
                                (std::string_view word) {
            const auto word_pos = word_to_document_freqs_.find(word);
            if (word_pos != word_to_document_freqs_.end()) {
                for (const auto [document_id, _] : word_pos->second) {
                    concurent_document_to_relevance.erase(static_cast<size_t>(document_id));
                }
            }
        };

        ForEach(policy, query.minus_words, erase_minus_func); // in concurrent_map.h

        std::map<int, double> document_to_relevance =
                concurent_document_to_relevance.BuildOrdinaryMap();

        std::vector<Document> matched_documents;
        matched_documents.reserve(document_to_relevance.size());

        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating}); //пушим id, relevance, rating найденных
        }
        return matched_documents;
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}




