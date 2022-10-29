#pragma once

#include <iostream>
#include <mutex>
#include <map>
#include <vector>
#include <execution>
#include <future>
#include <type_traits>
#include <iterator>
//#include <type_traits>

//using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

    size_t getBucketId(const Key& key) const {
        return static_cast<uint64_t>(key) % buckets_.size();
    }

    std::vector<Bucket> buckets_;

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"); //s

    //Структура Access должна вести себя так же, как в шаблоне Synchronized:
    //предоставлять ссылку на значение словаря и обеспечивать синхронизацию доступа к нему.
    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex)
            , ref_to_value(bucket.map[key]) {}
    };

    //Конструктор класса ConcurrentMap<Key, Value>
    //принимает количество подсловарей, на которые надо разбить всё пространство ключей.
    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    //operator[] должен вести себя так же, как аналогичный оператор у map:
    //если ключ key есть в словаре, должен возвращаться объект класса Access,
    // содержащий ссылку на соответствующее ему значение. Если key в словаре нет,
    //в него надо добавить пару (key, Value()) и вернуть объект класса Access, содержащий ссылку на только что добавленное значение.
    Access operator[](const Key& key) {
        auto& bucket = buckets_[getBucketId(key)];
        return Access{key, bucket};
    }

    void erase(const Key& key) {
        auto &bucket = buckets_[getBucketId(key)];
        std::lock_guard guard(bucket.mutex);
        bucket.map.erase(key);
    }

    //Метод BuildOrdinaryMap должен сливать вместе части словаря
    //и возвращать весь словарь целиком. При этом он должен быть потокобезопасным,
    //то есть корректно работать, когда другие потоки выполняют операции с ConcurrentMap
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }
};



