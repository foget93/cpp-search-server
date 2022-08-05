#include <search_server.h>
#include <cassert> //  не нужно
#include <cstdlib> // abort();
#include <iomanip>

using namespace std;

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
    }
    {
        SearchServer server;// обработка минус слов при матчинге
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [match_words, _] = server.MatchDocument("-cat", doc_id);
        ASSERT_HINT(match_words.empty(),"Minus word found"s);
    }
}

void TestSortingRelevance() {
    const int doc_id = 42;
    const int doc_id1 = 12;
    const int doc_id2 = 33;
    const string content = "cat in the city"s;
    const string content1 = "dog in the"s;
    const string content2 = "cat cat at home"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        //server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat home"s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_HINT(doc0.relevance > doc1.relevance, "Sorting is not correct"s); // убывание
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

void TestComputeRelevance() {
    SearchServer search_server("и в на"s);
    //search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto found_docs =  search_server.FindTopDocuments("пушистый ухоженный кот"s);
    const double EPSILON = 1e-6;
    ASSERT_HINT(found_docs[0].relevance - 0.866434 < EPSILON, "relevance calculation is wrong"s);
    ASSERT_HINT(found_docs[1].relevance - 0.173287 < EPSILON, "relevance calculation is wrong"s);
    ASSERT_HINT(found_docs[2].relevance - 0.173287 < EPSILON, "relevance calculation is wrong"s);
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
    //TestSearchServer();
    //cout << "Search server testing finished"s << endl;
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


