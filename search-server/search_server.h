#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

#include "document.h"
#include "string_processing.h"
#include <execution>
//using

const int MAX_RESULT_DOCUMENT_COUNT {5};

class SearchServer {
public:

    using DataAfterMatching = std::tuple<std::vector<std::string>, DocumentStatus>;
    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    //inline static constexpr int INVALID_DOCUMENT_ID = -1;

    SearchServer() = default; // старые тесты без стоп слов

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {} // Invoke delegating constructor from string container

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;
    // 3 перегрузка не нужна(значение по умолчанию status = DocumentStatus::ACTUAL)

    int GetDocumentCount() const;

    DataAfterMatching MatchDocument(const std::string& raw_query, int document_id) const;

    //int GetDocumentId(int index) const; //- отказ 5 спринт
    const std::map<std::string, double> &GetWordFrequencies(int index) const;

    void RemoveDocument(int index);

    template<typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int index);

    std::set<int>::iterator begin();

    std::set<int>::const_iterator begin() const;

    std::set<int>::iterator end();

    std::set<int>::const_iterator end() const;

private:
    struct DocumentData {
        int rating {0};
        DocumentStatus status {DocumentStatus::ACTUAL};
    };

    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_; // word -> id документов с word и их idf*tf
    std::map<int, DocumentData> documents_;  // id + средний рейтинг + статус

    //=========5 sprint======
    std::set<int> documents_ids_;
    std::map<int, std::map<std::string, double>> words_freqs_by_documents_;
    //=======================

    static bool IsValidWord(const std::string& word);

    bool IsStopWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const; // erase stop words

    static int ComputeAverageRating(const std::vector<int>& ratings); // считаем средний рейтинг

    struct QueryWord {
        std::string data;
        bool is_minus{false};
        bool is_stop{false};
    };

    QueryWord ParseQueryWord(std::string text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const ; //разбиваем на +- слова

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

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

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {

    const Query query = ParseQuery(raw_query);// исключения бросаются в ParseQueryWord
    std::vector<Document> result = FindAllDocuments(query, document_predicate);

    const double EPSILON = 1e-6;
    sort(result.begin(), result.end(),
         [EPSILON](const Document& lhs, const Document& rhs) {
            if(std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
         });
    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return result;
}
// ищем все доки по плюс минус словам запроса, и предикату или DocumentStatus, снизу перегрузки FindTopDocuments.

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance; // key: id, value: relevance
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);// compute IDF
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq; // idf*TF
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id); // delete for minus word
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating}); //пушим id, relevance, rating найденных
    }
    return matched_documents;
}

template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int index) {
    using namespace std::execution;

    auto it_doc_pos = documents_ids_.find(index);
    if (it_doc_pos == documents_ids_.end())
        return;

    std::vector<const std::string*> pointers;

    auto& words_freqs = words_freqs_by_documents_.at(index);

    std::transform(
                   words_freqs.begin(), words_freqs.end(),
                   pointers.begin(),
                   [](std::pair<const std::string, double>& word_freq){
                        return &(word_freq.first);
                    });
    std::for_each(policy,
                  pointers.begin(), pointers.end(),
                  [&](auto& pword) { // [word,_] not worked
                        word_to_document_freqs_.at(*pword).erase(index);
                    });

    documents_ids_.erase(it_doc_pos);
    documents_.erase(index);
    words_freqs_by_documents_.erase(index);
}

/*
        auto it_doc_pos = documents_ids_.find(index);

        if (it_doc_pos == documents_ids_.end())
            return;
        // map<int, map<string, double>> words_freqs_by_documents_ -> word : string
        for (const auto& [word, _] : words_freqs_by_documents_.at(index)) {
            // map<string, map<int, double>> word_to_document_freqs_ -> (erase - int)
            word_to_document_freqs_.at(word).erase(index);
        }
        documents_ids_.erase(it_doc_pos);
        documents_.erase(index);
        words_freqs_by_documents_.erase(index);
*/
