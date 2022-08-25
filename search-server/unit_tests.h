#pragma once

#include <iostream>
#include <utility>
#include <vector>
#include <set>
#include <map>
#include <string>

using namespace std::literals;

template <typename First, typename Second>
std::ostream& operator<<(std::ostream& out, const std::pair<First, Second>& container) {
    out << container.first <<": " << container.second;
    return out;
}

template <typename Container>
std::ostream& Print(std::ostream& out, Container container){
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
std::ostream& operator<<(std::ostream& out, const std::vector<Element>& container) {
    out <<'[';
    Print(out, container);
    out << ']';
    return out;
}

template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::set<Element>& container) {
    out <<'{';
    Print(out, container);
    out << '}';
    return out;
}

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::map<Key, Value>& container) {
    out <<'{';
    Print(out, container);
    out << '}';
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint);

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename FUNC>
void RunTestImpl(FUNC f, const std::string& name_func) {
        f();
        std::cerr << name_func << " OK" << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов

void TestExcludeStopWordsFromAddedDocumentContent();

void TestExcludeMinusWordsFromAddedDocumentContent();

void TestMatchDocument();

void TestSortingRelevance();

void TestComputeRatings();

void TestFilterWithPredicate();

void TestFindStatus();

void TestComputeRelevance();
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();

// --------- Окончание модульных тестов поисковой системы -------------------------------------------
