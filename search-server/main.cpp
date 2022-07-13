#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
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
    }

    return words;
}

struct Document {
    int id;
    int relevance;
};



class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }
//map<string, set<int>> word_to_documents_;
    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        //documents_.push_back({document_id, words});
        for ( const auto& word : words) {
            word_to_documents_[word].insert(document_id); //Вставьте в множество документов, соответствующих очередному слову документа, id вставляемого документа. Так очередной документ будет добавлен в инвертированный индекс.
        }
        //word_to_documents_
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
        });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    map<string, set<int>> word_to_documents_; // word +  инвертированный индекс документа
    set<string> stop_words_;

    //============methods================================================
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const{
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }


    QueryWord ParseQueryWord(string text) const {
        bool is_minus {false};

        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }



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
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        //map<string, set<int>> word_to_documents_;
        map<int, int> document_to_relevance; //ключ - id, значение - релевантность (кол-во плюс слов найденных в ней)
        for (const auto& word : query.plus_words) {
            if (word_to_documents_.count(word) == 0) {
                continue; //ненайденные пропускаем
            }
            for (const int document_id : word_to_documents_.at(word)) {
                ++document_to_relevance[document_id];
            }
            // Если в word_to_documents_ есть плюс-слово, увеличьте в document_to_relevance релевантности всех документов,
            //где это слово найдено. Так вы соберёте все документы, которые содержат плюс-слова запроса.
        }

        for (const string& word : query.minus_words) {
            if (word_to_documents_.count(word) == 0){
                continue;
            }
            for (const int document_id : word_to_documents_.at(word)) {
                document_to_relevance.erase(document_id);
            }
            //Исключите из результатов поиска все документы, в которых есть минус-слова.
            //В методе FindAllDocuments переберите в цикле все минус-слова поискового запроса.
            //Если в word_to_documents_ есть минус-слово, удалите из document_to_relevance все документы с этим минус-словом.
            //Так в document_to_relevance останутся только подходящие документы.
        }

        vector<Document> matched_documents;
        for (auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance});
        }
        return matched_documents;
        //Перенесите id и релевантности документов из document_to_relevance в vector<Document> и верните результирующий вектор.
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}

