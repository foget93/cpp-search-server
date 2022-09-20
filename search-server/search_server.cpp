#include "search_server.h"
#include "log_duration.h" // матчинг и поиск топ

#include <numeric>
#include <cmath>

using namespace std::literals;

    void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
        if (document_id < 0)
            throw std::invalid_argument("Попытка добавить документ с отрицательным id.");
        if (documents_.count(document_id) > 0)
            throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа.");

        for (const std::string& word : SplitIntoWords(document)) {
            if (!IsValidWord(word))
                throw std::invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.");
        } // проверка

        const std::vector<std::string> words = SplitIntoWordsNoStop(document);
        const double step = 1.0 / words.size();
        for (const std::string& word : words) {
            word_to_document_freqs_[word][document_id] += step;
            words_freqs_by_documents_[document_id][word] += step;
        }
        // example: words = "hello little cat", частота слова cat для этого документа 1/3;(for TF)
        // map<string, map<int, double>> word_to_document_freqs_;
        // map<int, map<string, double>> words_freqs_by_documents_; - по id
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        documents_ids_.insert(document_id);
    }

    std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {

        return FindTopDocuments(raw_query,
                                [status]([[maybe_unused]]int document_id, DocumentStatus status_predicate, [[maybe_unused]] int rating )
                                { return status == status_predicate; }
                               );
    }

    int SearchServer::GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
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
    }
// -------5 sprint delete----------
/*  int SearchServer::GetDocumentId(int index) const {
        if (index > static_cast<int>(documents_.size()) || index < 0)
            throw std::out_of_range("индекс переданного документа выходит за пределы допустимого диапазона [0; количество документов).");

        int local_index = 0;
        for (const auto& document : documents_) {
            if (local_index == index)
                return document.first;
            local_index++;
        }
        return -1; // для тестовой системы (не должно дойти)
        //метод at бросает исключение out_of_range =)
    }
*/
// -------5 sprint delete----------
    bool SearchServer::IsValidWord(const std::string& word) {
        // A valid word must not contain special characters
        return std::none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    bool SearchServer::IsStopWord(const std::string& word) const {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
        std::vector<std::string> words;
        for (const std::string& word : SplitIntoWords(text)) {
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

    SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
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


    SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
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
    } //разбиваем на +- слова

    double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
        return std::log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    } // IDF
    const std::map<std::string, double> &SearchServer::GetWordFrequencies(int index) const {
        static const std::map<std::string, double> empty_map {};

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
            word_to_document_freqs_.at(word).erase(index);
        }
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


