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

    template <typename ExecutionPolicy>
    DataAfterMatching MatchDocument(ExecutionPolicy&& policy, const std::string& raw_query, int document_id) const;

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

    template <typename ExecutionPolicy>
    Query ParseQuery(ExecutionPolicy&& policy, const std::string& text) const;

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


    auto& words_freqs = words_freqs_by_documents_.at(index);
    std::vector<const std::string* >  p_strs(words_freqs.size());

    std::transform(std::execution::par,
                   words_freqs.begin(), words_freqs.end(),
                   p_strs.begin(),
                   [](const auto& word_freq){//const auto& [word,_] not worked!!!!!????????????????????????S
                        return &(word_freq.first);
                    });

    std::for_each(policy,
                  p_strs.begin(), p_strs.end(),
                  [&](const auto& ptr_word) {
                        word_to_document_freqs_.at(*ptr_word).erase(index);
                    });

    documents_ids_.erase(it_doc_pos);
    documents_.erase(index);
    words_freqs_by_documents_.erase(index);
}

template <typename ExecutionPolicy>
SearchServer::Query SearchServer::ParseQuery(ExecutionPolicy&& policy, const std::string& text) const {
    using namespace std::execution;
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
//    Query query;
//    const std::vector<std::string> words =  SplitIntoWords(text);
//    std::vector<QueryWord> query_words(words.size());

//    std::transform(policy,
//                   words.begin(), words.end(),
//                   query_words.begin(),
//                   [&](const std::string& word){
//                        return std::move(ParseQueryWord(word));
//                     });

//    for (const auto& query_word : query_words) {
//        if (!query_word.is_stop) {
//            if (query_word.is_minus) {
//                query.minus_words.insert(query_word.data);
//            } else {
//                query.plus_words.insert(query_word.data);
//            }
//        }
//    }
    /*//почему не работает с par
    std::for_each(policy,
                  query_words.begin(), query_words.end(),
                  [&](const QueryWord& query_word){
                        if (!query_word.is_stop) {
                            if (query_word.is_minus) {
                                query.minus_words.insert(query_word.data);
                            } else {
                                query.plus_words.insert(query_word.data);
                            }
                        }
                    });*/


    return query;
}
/*    SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
        Query query;
        for (const std::string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    } //разбиваем на +- слова*/

template <typename ExecutionPolicy>
SearchServer::DataAfterMatching SearchServer::MatchDocument(ExecutionPolicy&& policy, const std::string& raw_query, int document_id) const{
     using namespace std::execution;

     const Query query = ParseQuery(policy, raw_query);  //мб сдедать паралельную версию ParseQuery +
     std::vector<std::string> matched_words;

     for (const std::string& word : query.plus_words) {
         if (word_to_document_freqs_.count(word) == 0) {
             continue;
         }
         if (word_to_document_freqs_.at(word).count(document_id)) {
             matched_words.push_back(word);
         }
     }

     for (const std::string& word : query.minus_words) {
         if (word_to_document_freqs_.count(word) == 0) {
             continue;
         }
         if (word_to_document_freqs_.at(word).count(document_id)) {
             matched_words.clear();
             break;
         }
     }
     return {matched_words, documents_.at(document_id).status};
}
/*
 *  func return tuple<vector<string>, DocumentStatus>
 *
    const Query query = ParseQuery(raw_query); // исключения бросаются в ParseQueryWord
    std::vector<std::string> matched_words;

    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
*/
