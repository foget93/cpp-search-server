#include "process_queries.h"
#include <execution>
#include <functional>

std::vector<std::vector<Document>>ProcessQueries (const SearchServer& search_server,
                                                     const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result(queries.size());

    std::transform(std::execution::par,
                   queries.begin(), queries.end(),
                   result.begin(),
                   [&search_server](const std::string& query) {
                        return search_server.FindTopDocuments(query);
                   });
    return result;
}

std::vector<std::vector<Document>>TrivialProcessQueries (const SearchServer& search_server,
                                                     const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result(queries.size());

    for (const std::string& query : queries) {
        result.push_back(search_server.FindTopDocuments(query));
    }
    return result;
}

std::vector<Document> ProcessQueriesJoined( const SearchServer& search_server,
                                            const std::vector<std::string>& queries) {

    auto responses = ProcessQueries(search_server, queries);
    size_t doc_count = std::transform_reduce(std::execution::par,
                                             responses.begin(), responses.end(),
                                             0u,
                                             std::plus<>(),
                                             [](const std::vector<Document>& response){
                                                return response.size();
                                             });
    std::vector<Document> result;
    result.reserve(doc_count);
    for (const auto& response : responses) {
        //std::move(response.begin(), response.end(), std::back_inserter(result));
        for (const auto& doc : response) {
            result.push_back(std::move(doc));
        }
    }
    return result;
}

std::vector<Document> TrivialProcessQueriesJoined( const SearchServer& search_server,
                                            const std::vector<std::string>& queries) {

    auto responses = TrivialProcessQueries(search_server, queries);
    size_t doc_count {0};
    for (const auto& response : responses) {
        doc_count += response.size();
    }

    std::vector<Document> result;

    result.reserve(doc_count);

    for (const auto& response : responses) {
        for (const auto& doc : response) {
            result.push_back(std::move(doc));
        }
    }
    return result;
}
