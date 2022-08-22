#ifndef SEARCH_SERVER_H
#define SEARCH_SERVER_H

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <deque>

using namespace std;


const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
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
} //string to vector<string> ->" "

struct Document {

    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

ostream& operator<<(ostream& out, const Document& document) {
    out << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s;
    return out;
}

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    //inline static constexpr int INVALID_DOCUMENT_ID = -1;

    SearchServer() = default; // старые тесты без стоп слов

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        if (any_of(stop_words_.cbegin(), stop_words_.cend(),
                  [](const string& word){ return !IsValidWord(word); } )) {
            throw invalid_argument("Стоп-слово содержит недопустимые символы!");
        } // https://en.cppreference.com/w/cpp/algorithm/all_any_none_of
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {   
    }
/*
     void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    } //setter -> add set<string> stop_words_
*/
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0)
            throw invalid_argument("Попытка добавить документ с отрицательным id.");
        if (documents_.count(document_id) > 0)
            throw invalid_argument("Попытка добавить документ c id ранее добавленного документа.");

        for (const string& word : SplitIntoWords(document)) {
            if (!IsValidWord(word))
                throw invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.");
        } // проверка

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double step = 1.0 / words.size();
        for (const string& word : words) {    
                word_to_document_freqs_[word][document_id] += step;
            // тут мб надо 2 цикла, сначала проверить все слова потом считать частоту
        }
        // example: words = "hello little cat", частота слова cat для этого документа 1/3;(for TF)
        // map<string, map<int, double>> word_to_document_freqs_;
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {

        const Query query = ParseQuery(raw_query);// исключения бросаются в ParseQueryWord
        vector<Document> result = FindAllDocuments(query, document_predicate);

        const double EPSILON = 1e-6;
        sort(result.begin(), result.end(),
             [EPSILON](const Document& lhs, const Document& rhs) {
                if(abs(lhs.relevance - rhs.relevance) < EPSILON)
                    return lhs.rating > rhs.rating;
                else
                    return lhs.relevance > rhs.relevance;
             });
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return result;

    } // ищем все доки по плюс минус словам запроса, и предикату или DocumentStatus, снизу перегрузки FindTopDocuments

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {

        return FindTopDocuments(raw_query,
                                [status](int document_id, DocumentStatus status_predicate, int rating )
                                { return status == status_predicate; }
                               );
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query); // исключения бросаются в ParseQueryWord

        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }

        for (const string& word : query.minus_words) {
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

    int GetDocumentId(int index) const {
        if (index > static_cast<int>(documents_.size()) || index < 0)
            throw out_of_range("индекс переданного документа выходит за пределы допустимого диапазона [0; количество документов).");

        int local_index = 0;
        for (const auto& document : documents_) {
            if (local_index == index)
                return document.first;
            local_index++;
        }
        return -1; // для тестовой системы (не должно дойти)
        //метод at бросает исключение out_of_range =)
    }

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status = DocumentStatus::ACTUAL;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;// word -> id документов с word и их idf*tf
    map<int, DocumentData> documents_; // id + средний рейтинг + статус

    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    } // erase stop words

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(),ratings.end(), 0);//review
        return rating_sum / static_cast<int>(ratings.size());
    } // считаем средний рейтинг

    struct QueryWord {
        string data;
        bool is_minus{false};
        bool is_stop{false};
    };

    QueryWord ParseQueryWord(string text) const {
        if (!IsValidWord(text))
            throw invalid_argument("В словах поискового запроса есть недопустимые символы с кодами от 0 до 31.");
        if (text == "-"s)
            throw invalid_argument("Отсутствие текста после символа «минус»: в поисковом запросе.");
        if (text.size() > 1)
            if (text[1] == '-' && text[0] == '-')
                throw invalid_argument("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах");
       // проверка
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    } // IDF

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance; // key: id, value: relevance
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id); //delete for minus word
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating}); //пушим id, relevance, rating найденных
        }
        return matched_documents;
    }
};


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : first_(begin)
        , last_(end)
        , size_(distance(first_, last_)) {
    }

    Iterator begin() const {
        return first_;
    }

    Iterator end() const {
        return last_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator first_, last_;
    size_t size_;
};

template <typename Iterator>
ostream& operator<<(ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t left = distance(begin, end); left > 0;) {
            const size_t current_page_size = min(page_size, left);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_.push_back({begin, current_page_end});

            left -= current_page_size;
            begin = current_page_end;
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}


class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , no_results_requests_(0)
        , current_time_(0) {
    }
    // сделаем "обертки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        AddRequest(result.size());
        return result;
    }
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
        const auto result = search_server_.FindTopDocuments(raw_query, status);
        AddRequest(result.size());
        return result;
    }
    vector<Document> AddFindRequest(const string& raw_query) {
        const auto result = search_server_.FindTopDocuments(raw_query);
        AddRequest(result.size());
        return result;
    }
    int GetNoResultRequests() const {
        return no_results_requests_;
    }
private:
    struct QueryResult {
        uint64_t timestamp{0};
        int results{0};
    };
    deque<QueryResult> requests_;
    const SearchServer& search_server_;
    int no_results_requests_;
    uint64_t current_time_;
    const static int min_in_day_ = 1440;

    void AddRequest(int results_num) {
        // новый запрос - новая секунда
        ++current_time_;
        // удаляем все результаты поиска, которые устарели
        while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
            if (0 == requests_.front().results) {
                --no_results_requests_;
            }
            requests_.pop_front();
        }
        // сохраняем новый результат поиска
        requests_.push_back({current_time_, results_num});
        if (0 == results_num) {
            ++no_results_requests_;
        }
    }
};
#endif // SEARCH_SERVER_H
