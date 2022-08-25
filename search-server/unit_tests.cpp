#include "unit_tests.h"
#include "search_server.h"

#include <cmath>

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
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
    const std::string content = "cat in -the city"s;
    const std::vector<int> ratings = {1, 2, 3};
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
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
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
    const std::string content1 = "cat in the city"s;
    const std::string content2 = "dog in the"s;
    const std::string content3 = "cat cat at home"s;
    const std::string content4 = "dog in home"s;
    const std::vector<int> ratings = {1, 2, 3};

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
    const std::string content = "cat at home"s;
    const std::vector<int> ratings = {11, 22, 33};
    const std::vector<int> hard_ratings = {1, 2, 3, 232, 132, 2};
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
                                                           [](int document_id, [[maybe_unused]]DocumentStatus status, [[maybe_unused]]int rating) {
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
    SearchServer search_server;

    search_server.AddDocument(0, "белый кот"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый белый кот"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пес выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});

    const auto found_docs =  search_server.FindTopDocuments("кот"s);
    const double EPSILON = 1e-6;

    double IDF = std::log(3.0/2);  //IDF слова "кот" (для каждого слова запроса)
                              //3 -> кол-во документов; 2 -> в 2х док-тах встречается слово "кот".

    double TF_0 = 1.0/2;
    double TF_1 = 1.0/3;
    double TF_2 = 0.0/4;      // (кол-во слов "кот" в документе) / (кол-во слов в документе)

    double IDFxTF_0 = IDF * TF_0; //~0.2027325540540822
    double IDFxTF_1 = IDF * TF_1; //~0.13515503603605478
    [[maybe_unused]]double IDFxTF_2 = IDF * TF_2; // 0 -> слово отсутствует

    double a = std::abs(found_docs[0].relevance - IDFxTF_0);
    double b = std::abs(found_docs[1].relevance - IDFxTF_1);

    ASSERT_HINT(a < EPSILON, "relevance calculation is wrong"s);
    ASSERT_HINT(b < EPSILON, "relevance calculation is wrong"s);
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
