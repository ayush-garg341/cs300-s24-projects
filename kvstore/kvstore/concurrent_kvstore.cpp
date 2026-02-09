#include "concurrent_kvstore.hpp"

#include <mutex>
#include <optional>

bool ConcurrentKvStore::Get(const GetRequest* req, GetResponse* res) {
    // TODO (Part A, Step 3 and Step 4): Implement!

    size_t b = this->store.bucket(req->key);
    auto result = this->store.getIfExists(b, req->key);
    if(result == std::nullopt)
    {
        return false;
    }
    res->value = result->value;
    return true;
}

bool ConcurrentKvStore::Put(const PutRequest* req, PutResponse*) {
    // TODO (Part A, Step 3 and Step 4): Implement!

    size_t b = this->store.bucket(req->key);
    this->store.insertItem(b, req->key, req->value);
    return true;
}

bool ConcurrentKvStore::Append(const AppendRequest* req, AppendResponse*) {
    // TODO (Part A, Step 3 and Step 4): Implement!
    size_t b = this->store.bucket(req->key);
    auto result = this->store.getIfExists(b, req->key);
    if(result == std::nullopt)
    {
        result->value = "";
    }
    result->value += req->value;
    this->store.insertItem(b, req->key, result->value);
    return true;
}

bool ConcurrentKvStore::Delete(const DeleteRequest* req, DeleteResponse* res) {
    // TODO (Part A, Step 3 and Step 4): Implement!

    size_t b = this->store.bucket(req->key);
    auto result = this->store.getIfExists(b, req->key);
    if(result == std::nullopt) {
        return false;
    }
    res->value = result->value;
    bool deleted = this->store.removeItem(b, req->key);
    return deleted;
}

bool ConcurrentKvStore::MultiGet(const MultiGetRequest* req,
                                 MultiGetResponse* res) {
    // TODO (Part A, Step 3 and Step 4): Implement!
    res->values.clear();

    for(size_t i = 0; i < req->keys.size(); ++i)
    {
        std::string key = req->keys[i];
        size_t b = this->store.bucket(key);
        auto val_it = this->store.getIfExists(b, key);
        if(val_it == std::nullopt)
        {
            return false;
        }
        res->values.push_back(val_it->value);
    }
    return true;
}

bool ConcurrentKvStore::MultiPut(const MultiPutRequest* req,
                                 MultiPutResponse*) {
    // TODO (Part A, Step 3 and Step 4): Implement!
    if(req->keys.size() != req->values.size())
    {
        return false;
    }

    for(size_t i = 0; i < req->keys.size(); ++i)
    {
        auto key = req->keys[i];
        auto val = req->values[i];
        size_t b = this->store.bucket(key);
        this->store.insertItem(b, key, val);
    }

    return true;
}

std::vector<std::string> ConcurrentKvStore::AllKeys() {
    // TODO (Part A, Step 3 and Step 4): Implement!
    std::vector<std::string> allkeys;
    for (size_t i = 0; i < this->store.buckets.size(); ++i) {
        auto& bucket = this->store.buckets[i];

        for (auto& item : bucket) {
            allkeys.push_back(item.key);
        }
    }

    return allkeys;
}
