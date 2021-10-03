#ifndef AFINA_STORAGE_STRIPED_LRU_H
#define AFINA_STORAGE_STRIPED_LRU_H

#include <map>
#include <mutex>
#include <string>
#include <functional>

#include "ThreadSafeSimpleLRU.h"

namespace Afina {
namespace Backend {

/**
 * # SimpleLRU thread safe version
 *
 *
 */
class StripedLRU : public Afina::Storage {
public:
    StripedLRU(size_t max_size = 1024, size_t amt_stripes = 4) {
        if (amt_stripes > 100) {
            throw std::runtime_error("Too many stripes wanted!\n");
        }
        _max_size = max_size;
        _num_shards = amt_stripes;
        for (int i = 0; i < _num_shards; i = i + 1) {
            _mas_of_storages.push_back(std::unique_ptr<ThreadSafeSimplLRU>(new ThreadSafeSimplLRU(_max_size/_num_shards)));
        }
    }
    ~StripedLRU() {} //vector will delete all unique_ptrs, and unique_ptr will delete its object 
    

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        size_t tek_hash = std::hash<std::string>{}(key);
        return _mas_of_storages[tek_hash % _num_shards]->Put(key,value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        size_t tek_hash = std::hash<std::string>{}(key);
        return _mas_of_storages[tek_hash % _num_shards]->PutIfAbsent(key,value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        size_t tek_hash = std::hash<std::string>{}(key);
        return _mas_of_storages[tek_hash % _num_shards]->Set(key,value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        size_t tek_hash = std::hash<std::string>{}(key);
        return _mas_of_storages[tek_hash % _num_shards]->Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        size_t tek_hash = std::hash<std::string>{}(key);
        return _mas_of_storages[tek_hash % _num_shards]->Get(key,value);
    }

private:
    std::size_t _max_size;
    std::size_t _num_shards;
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> _mas_of_storages; //doesn't work without ptrs
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_STRIPED_LRU_H
