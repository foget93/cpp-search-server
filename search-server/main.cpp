#include <cassert> //  не нужно
#include <cstdlib> // abort();
#include <iomanip>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

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
        for (const auto& stop_word : MakeUniqueNonEmptyStrings(stop_words))
        {
            if (!IsValidWord(stop_word))
                throw invalid_argument("Стоп-слово содержит недопустимые символы!");
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
        for (const auto& stop_word : SplitIntoWords(stop_words_text))
        {
            if (!IsValidWord(stop_word))
                throw invalid_argument("Стоп-слово содержит недопустимые символы.");
        }
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

        for (const string& word : SplitIntoWords(raw_query)) {
            if (!IsValidWord(word))
                throw invalid_argument("В словах поискового запроса есть недопустимые символы с кодами от 0 до 31.");
            if (word == "-"s)
                throw invalid_argument("Отсутствие текста после символа «минус»: в поисковом запросе.");
            if (word.size() > 1)
                if (word[1] == '-' && word[0] == '-')
                    throw invalid_argument("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах"); //пустой optional
        } // проверка

        const Query query = ParseQuery(raw_query);
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
        for (const string& word : SplitIntoWords(raw_query)) {
            if (!IsValidWord(word))
                throw invalid_argument("В словах поискового запроса есть недопустимые символы с кодами от 0 до 31.");
            if (word == "-"s)
                throw invalid_argument("Отсутствие текста после символа «минус»: в поисковом запросе.");
            if (word.size() > 1)
                if (word[1] == '-' && word[0] == '-')
                    throw invalid_argument("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах");
        }

        const Query query = ParseQuery(raw_query);

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
        //метод at бросает исключение out_of_range =)
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
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
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text)};
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

// --------- for tests -------------------------------------------
template <typename First, typename Second>
ostream& operator<<(ostream& out, const pair<First, Second>& container) {
    out << container.first <<": " << container.second;
    return out;
}

template <typename Container>
ostream& Print(ostream& out, Container container){
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out <<'[';
    Print(out, container);
    out << ']';
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    out <<'{';
    Print(out, container);
    out << '}';
    return out;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out <<'{';
    Print(out, container);
    out << '}';
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename FUNC>
void RunTestImpl(FUNC f, const string& name_func) {
        f();
        cerr << name_func << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server("if"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestExcludeMinusWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in -the city"s;
    const vector<int> ratings = {1, 2, 3};
        //Убеждаемся, что поиск минус слова возвращает пустой результат
    {
         SearchServer server;
         server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
         ASSERT_HINT(server.FindTopDocuments("the"s).empty(), "Minus words must be excluded from documents"s);
    }
    {
        //запрос с минус словами
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("-in dog"s).empty(), "Request with minus word is not empty."s);
    }
}

void TestMatchDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto [match_words, _] = server.MatchDocument(content, doc_id);
        const auto vec_str = SplitIntoWords(content);
        for (int i = 0; i < static_cast<int>(vec_str.size()); i++ )
            ASSERT_HINT(find(match_words.begin(), match_words.end(), vec_str[i]) != match_words.end(), "Matches not found"s);
        //проверка на количество возвращенных слов, что бы убедиться, что нет лишних слов или дублей
        ASSERT_EQUAL_HINT(match_words.size(), vec_str.size(), "Content.size() != match_words.size"s);
    }
    {
        SearchServer server;// обработка минус слов при матчинге
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [match_words, _] = server.MatchDocument("-cat", doc_id);
        ASSERT_HINT(match_words.empty(),"Minus word found"s);
    }
}

void TestSortingRelevance() {
    const string content1 = "cat in the city"s;
    const string content2 = "dog in the"s;
    const string content3 = "cat cat at home"s;
    const string content4 = "dog in home"s;
    const vector<int> ratings = {1, 2, 3};

    {
        //надо обрабатывать минимум три случая при проверке сортировки,
        //два элемента с высокой вероятностью случайно могут стать в нужном порядке,
        //так же порядок добавления документов не должен совпадать с итоговым списком.
        SearchServer server;
        //server.SetStopWords("in the"s);
        server.AddDocument(1, content1, DocumentStatus::ACTUAL, ratings); // #3
        server.AddDocument(2, content2, DocumentStatus::ACTUAL, ratings); // not found
        server.AddDocument(3, content3, DocumentStatus::ACTUAL, ratings); // #1
        server.AddDocument(4, content4, DocumentStatus::ACTUAL, ratings); // #2

        const auto found_docs = server.FindTopDocuments("cat home"s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        const Document& doc2 = found_docs[2];
        ASSERT_HINT(doc0.relevance >= doc1.relevance &&  doc1.relevance >= doc2.relevance, "Sorting is not correct"s); // убывание
    }
}

void TestComputeRatings() {
    const int doc_id = 12;
    const int doc_id1 = 22;
    const string content = "cat at home"s;
    const vector<int> ratings = {11, 22, 33};
    const vector<int> hard_ratings = {1, 2, 3, 232, 132, 2};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, "cat msadsad"s, DocumentStatus::ACTUAL, hard_ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs[0].rating, 62); //сумма (1 + 2 + 3 + 232 + 132 + 2) / 6 = 62
        ASSERT_EQUAL(found_docs[1].rating, 22);
    }

}

void TestFilterWithPredicate() {
    SearchServer search_server("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                                           [](int document_id, DocumentStatus status, int rating) {
                                                           return document_id % 2 == 0; });
    for (const Document& document : found_docs) {
        ASSERT_EQUAL(document.id % 2, 0);
    }
}

void TestFindStatus() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto found_docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_HINT(found_docs.size() == 1u && found_docs[0].id == 3, "FindTopDocuments by DocumentStatus:: is not correct."s
                                                                  " Rigth value: found_docs.size() == 1 && found_docs[0].id == 3"s);
}

template<typename Documents, typename Term>
vector<double> ComputeTfIdfs(const Documents& documents,
                             const Term& term) {
    vector<double> tf_idfs;

        int document_freq = 0;
        for (const auto& document : documents) {
            const int freq = count(document.begin(), document.end(), term);
            if (freq > 0) {
                ++document_freq;
            }
            tf_idfs.push_back(static_cast<double>(freq) / document.size());
        }

        const double idf = log(static_cast<double>(documents.size()) / document_freq);
        for (double& tf_idf : tf_idfs) {
            tf_idf *= idf;
        }

        return tf_idfs;
}//for TestComputeRelevance()

vector<double> operator+(const vector<double>& vec_1, const vector<double>& vec_2) {
    vector<double> result(vec_1.size());
    if (vec_1.size() == vec_2.size())
    {
        for (int i = 0; i < static_cast<int>(vec_1.size()); i++)
            result[i] = vec_1[i] + vec_2[i];
    }
    return result;
}//for TestComputeRelevance()

void TestComputeRelevance() {
    //ComputeTfIdfs и operator+(векторов) реализованы выше

    const vector<vector<string>> documents = {
        {"пушистый"s, "кот"s, "пушистый"s, "хвост"s}, //relevance = 0.6506724213610959
        {"ухоженный"s, "пёс"s, "выразительные"s, "глаза"s},//relevance = 0.27465307216702745
        {"белый"s, "кот"s, "и"s, "модный"s, "ошейник"s} //relevance = 0.08109302162163289
    };

    const string query = {"пушистый ухоженный кот"s};

    const auto vec_query = SplitIntoWords(query);
    const auto& tf_idfs = ComputeTfIdfs(documents, vec_query[0]) + //пушистый
                          ComputeTfIdfs(documents, vec_query[1]) + //ухоженный
                          ComputeTfIdfs(documents, vec_query[2]);  //кот


    SearchServer search_server;
    //search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});


    const auto found_docs =  search_server.FindTopDocuments(query);
    const double EPSILON = 1e-6;

    double a = abs(found_docs[0].relevance - tf_idfs[0]);
    double b = abs(found_docs[1].relevance - tf_idfs[1]);
    double c = abs(found_docs[2].relevance - tf_idfs[2]);

    ASSERT_HINT(a < EPSILON, "relevance calculation is wrong"s);
    ASSERT_HINT(b < EPSILON, "relevance calculation is wrong"s);
    ASSERT_HINT(c < EPSILON, "relevance calculation is wrong"s);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortingRelevance);
    RUN_TEST(TestComputeRatings);
    RUN_TEST(TestFilterWithPredicate);
    RUN_TEST(TestFindStatus);
    RUN_TEST(TestComputeRelevance);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -------------------------------------------
int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;

// --------- Окончание модульных тестов в маин -------------

    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);

    return 0;
}


