#include <iomanip>

#include "request_queue.h"
#include "paginator.h"
#include "unit_tests.h"
#include "remove_duplicates.h"
#include "test_example_functions.h"
#include "log_duration.h"
#include "search_server.h"
#include "process_queries.h"

#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <execution>



using namespace std;
/*
//ProcessQueries
//ProcessQueriesJoined
string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int max_word_count) {
    const int word_count = uniform_int_distribution(1, max_word_count)(generator);
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}
template <typename QueriesProcessor>
void Test(string_view mark, QueriesProcessor processor, const SearchServer& search_server, const vector<string>& queries) {
    LOG_DURATION(mark);
    const auto documents_lists = processor(search_server, queries);
}

#define TEST(processor) Test(#processor, processor, search_server, queries)
*/
// бенчмарк на удаление========================================================================
string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int max_word_count) {
    const int word_count = uniform_int_distribution(1, max_word_count)(generator);
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    for (int id = 0; id < document_count; ++id) {
        search_server.RemoveDocument(policy, id);
    }
    cout << search_server.GetDocumentCount() << endl;
}

#define TEST(mode) Test(#mode, search_server, execution::mode)

int main() {
/*
    mt19937 generator;
    const auto dictionary = GenerateDictionary(generator, 2'000, 25);
    const auto documents = GenerateQueries(generator, dictionary, 20'000, 10);
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }
    const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
    TEST(ProcessQueries);
    TEST(ProcessQueriesJoined);
    TEST(ProcessQueriesJoined_list);
    TEST(TrivialProcessQueries);
    TEST(TrivialProcessQueriesJoined);
*/
/*
    //ProcessQueries
    //ProcessQueriesJoined
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
        cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
    }
    cout << endl;
    for (const Document& document : ProcessQueriesJoined_list(search_server, queries)) {
        cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
    }

    id = 0;
    for (const auto& documents : ProcessQueries(search_server, queries) ) {
        cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
    }*/

    // removed
    /*SearchServer search_server("and with"s);

       int id = 0;
       for (
           const string& text : {
               "funny pet and nasty rat"s,
               "funny pet with curly hair"s,
               "funny pet and not very nasty rat"s,
               "pet with rat and rat and rat"s,
               "nasty rat with curly hair"s,
           }
       ) {
           search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
       }

       const string query = "curly and funny"s;

       auto report = [&search_server, &query] {
           cout << search_server.GetDocumentCount() << " documents total, "s
               << search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << endl;
       };

       report();
       // однопоточная версия
       search_server.RemoveDocument(5);
       report();
       // однопоточная версия
       search_server.RemoveDocument(execution::seq, 1);
       report();
       // многопоточная версия
       search_server.RemoveDocument(execution::par, 2);
       report();
*/

    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 10'000, 25);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

    {
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST(seq);
    }
    {
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST(par);
    }
    return 0;
}

