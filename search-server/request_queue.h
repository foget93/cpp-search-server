#pragma once

#include <deque>
//#include <vector>
//#include <string> - в "document.h"

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
        , no_results_requests_(0)
        , current_time_(0) {}

    // сделаем "обертки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        AddRequest(result.size());
        return result;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        uint64_t timestamp {0};
        int results {0};
    };

    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;
    int no_results_requests_ {0};
    uint64_t current_time_ {0};
    const static int min_in_day_ {1440};

    void AddRequest(int results_num);
};
