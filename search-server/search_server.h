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

//using

const int MAX_RESULT_DOCUMENT_COUNT {5};

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

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;
    // 3 перегрузка не нужна(значение по умолчанию status = DocumentStatus::ACTUAL)

    int GetDocumentCount() const;

    DataAfterMatching MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const {
        return MatchDocument(raw_query, document_id);}

    DataAfterMatching MatchDocument(std::string_view raw_query, int document_id) const;

    template <typename ExecutionPolicy>
    DataAfterMatching MatchDocument(ExecutionPolicy&& parallel_policy, std::string_view raw_query, int document_id) const;

    //int GetDocumentId(int index) const; //- отказ 5 спринт
    const std::map<std::string_view, double> &GetWordFrequencies(int index) const;

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

    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_; // word -> id документов с word и их idf*tf
    std::map<int, DocumentData> documents_;  // id + средний рейтинг + статус
    std::set<int> documents_ids_;
    std::map<int, std::map<std::string_view, double>> words_freqs_by_documents_;
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
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const ; //разбиваем на +- слова

    QueryWord ParseQueryWord(std::string_view text) const; // mb bool

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

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
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {

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
    std::vector<const std::string*>  p_strs(words_freqs.size());

    std::transform(policy/*std::execution::par*/,
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
SearchServer::DataAfterMatching SearchServer::MatchDocument(ExecutionPolicy&& parallel_policy , std::string_view raw_query, int document_id) const {
    using namespace std::execution;

    std::vector<std::string_view> plus_words;
    std::vector<std::string_view> minus_words;

    //std::vector<std::string_view> words = SplitIntoWords(raw_query);
    for (std::string_view word : SplitIntoWords(raw_query)) {
      const QueryWord query_word = ParseQueryWord(word);
      if (!query_word.is_stop) {
          if (query_word.is_minus) {
              minus_words.push_back(query_word.data.substr(0, query_word.data.length()));
          } else { 
              plus_words.push_back(query_word.data.substr(0, query_word.data.length()));
          }
      }
    }

    const auto ckeck_word = [this, document_id](std::string_view word) {
        const auto pos = word_to_document_freqs_.find(word);
        return pos != word_to_document_freqs_.end() && pos->second.count(document_id);
    };

    //check minus
    if (std::any_of(parallel_policy, minus_words.begin(), minus_words.end(), ckeck_word)) {
        return {/*std::vector<std::string_view>*/ {}, documents_.at(document_id).status};
    }

    //std::vector<std::string> matched_words(plus_words.size());
    std::vector<std::string_view> matched_words(plus_words.size());

    auto it_end_cpy =
          std::copy_if(parallel_policy, plus_words.begin(), plus_words.end(),
                      matched_words.begin(),
                      [this, &matched_words, &ckeck_word](std::string_view word) {
                           return ckeck_word(word);
                      });

    matched_words.resize(std::distance(matched_words.begin(), it_end_cpy));

//  Remove duplicates
    std::sort(parallel_policy, matched_words.begin(), matched_words.end());
    auto last = std::unique(parallel_policy, matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());

//    std::vector<std::string_view> matched_words_view(matched_words.size());
//    std::copy(parallel_policy, matched_words.begin(), matched_words.end(), matched_words_view.begin());

    return {/*matched_words_view*/matched_words, documents_.at(document_id).status};

}
/*
 *  func return tuple<vector<string>, DocumentStatus>
*/
