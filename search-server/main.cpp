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

/* Подставьте вашу реализацию класса SearchServer сюда */

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> documents;
        ASSERT_EQUAL(server.FindTopDocuments("in"s, documents), true);

        ASSERT_EQUAL(documents.size(), 1u);
        const Document& doc0 = documents[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"s);
        //server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> documents;
        (void)server.FindTopDocuments("in"s, documents);
        ASSERT_HINT(documents.empty(), "Stop words must be excluded from documents"s);
    }
}
/*
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
    //search_server.SetStopWords("и в на"s);

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
*/
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
//    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent);
//    RUN_TEST(TestMatchDocument);
//    RUN_TEST(TestSortingRelevance);
//    RUN_TEST(TestComputeRatings);
//    RUN_TEST(TestFilterWithPredicate);
//    RUN_TEST(TestFindStatus);
//    RUN_TEST(TestComputeRelevance);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -------------------------------------------

int main() {
    // Инициализируем поисковую систему, передавая стоп-слова в контейнере vector
   /* const vector<string> stop_words_vector = {"и"s, "в"s, "на"s, ""s, "в"s};
    SearchServer search_server1(stop_words_vector);

    // Инициализируем поисковую систему передавая стоп-слова в контейнере set
    const set<string> stop_words_set = {"и"s, "в"s, "на"s};
    SearchServer search_server2(stop_words_set);

    // Инициализируем поисковую систему строкой со стоп-словами, разделёнными пробелами
    SearchServer search_server3("  и  в на   "s);
*/
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
// --------- Окончание модульных тестов в маин -------------


    SearchServer search_server("и в на"s);

    // Явно игнорируем результат метода AddDocument, чтобы избежать предупреждения
    // о неиспользуемом результате его вызова
    (void) search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});

    if (!search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2})) {
        cout << "Документ не был добавлен, так как его id совпадает с уже имеющимся"s << endl;
    }

    if (!search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2})) {
        cout << "Документ не был добавлен, так как его id отрицательный"s << endl;
    }

    if (!search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, {1, 3, 2})) {
        cout << "Документ не был добавлен, так как содержит спецсимволы"s << endl;
    }

    vector<Document> documents;
    if (search_server.FindTopDocuments("--пушистый"s, documents)) {
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    } else {
        cout << "Ошибка в поисковом запросе"s << endl;
    }

    if (search_server.GetDocumentId(-1) != -1)
    {
        cout << "ERROR" << endl;
    }

    if (search_server.GetDocumentId(0) == 1)
        cout << "OK" << endl;
    return 0;
}


/* ==================test ASSERT_EQUAL for set, vector....
vector<int> TakeEvens(const vector<int>& numbers) {
    vector<int> evens;
    for (int x : numbers) {
        if (x % 2 == 0) {
            evens.push_back(x);
        }
    }
    return evens;
}

map<string, int> TakeAdults(const map<string, int>& people) {
    map<string, int> adults;
    for (const auto& [name, age] : people) {
        if (age >= 18) {
            adults[name] = age;
        }
    }
    return adults;
}

bool IsPrime(int n) {
    if (n < 2) {
        return false;
    }
    int i = 2;
    while (i * i <= n) {
        if (n % i == 0) {
            return false;
        }
        ++i;
    }
    return true;
}

set<int> TakePrimes(const set<int>& numbers) {
    set<int> primes;
    for (int number : numbers) {
        if (IsPrime(number)) {
            primes.insert(number);
        }
    }
    return primes;
}
*/

/* =================test ASSERT_EQUAL for set, vector....
 * {
     const set<int> numbers = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
     const set<int> expected_primes = {2, 3, 5, 7, 11, 13};
     ASSERT_EQUAL(TakePrimes(numbers), expected_primes);
 }

 {
     const map<string, int> people = {{"Ivan"s, 19}, {"Sergey"s, 16}, {"Alexey"s, 18}};
     const map<string, int> expected_adults = {{"Alexey"s, 18}, {"Ivan"s, 19}};
     ASSERT_EQUAL(TakeAdults(people), expected_adults);
 }

 {
     const vector<int> numbers = {3, 2, 1, 0, 3, 6};
     const vector<int> expected_evens = {2, 0, 6};
     ASSERT_EQUAL(TakeEvens(numbers), expected_evens);
 }*/

/*int main() { //old main

        SearchServer search_server;
        search_server.SetStopWords("и в на"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

        cout << "ACTUAL by default:"s << endl;
        for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
            PrintDocument(document);
        }

        cout << "BANNED:"s << endl;
        for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
            PrintDocument(document);
        }

        cout << "Even ids:"s << endl;
        for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s,
                                                                       [](int document_id, DocumentStatus status, int rating) {
                                                                       return document_id % 2 == 0; })) {
            PrintDocument(document);
        }
        return 0;
}*/
