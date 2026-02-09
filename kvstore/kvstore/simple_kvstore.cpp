#include "simple_kvstore.hpp"

bool SimpleKvStore::Get(const GetRequest* req, GetResponse* res) {
    // TODO (Part A, Step 1 and Step 2): Implement!

    std::lock_guard<std::mutex> lock(mtx);
    auto it = store.find(req->key);
    if(it == store.end())
    {
        return false;
    }
    auto& vec = it->second;
    std::string joint_val;
    for (auto& f : vec)
        joint_val += f;

    res->value = joint_val;
    return true;
}

bool SimpleKvStore::Put(const PutRequest* req, PutResponse*) {
    // TODO (Part A, Step 1 and Step 2): Implement!

    std::lock_guard<std::mutex> lock(mtx);
    store[req->key] = { req->value };
    return true;
}

bool SimpleKvStore::Append(const AppendRequest* req, AppendResponse*) {
    // TODO (Part A, Step 1 and Step 2): Implement!

    std::lock_guard<std::mutex> lock(mtx);
    auto it = store.find(req->key);

    // Key does not exist
    if(it == store.end())
    {
        store[req->key] = { req->value };
    }
    else {
        store[req->key].push_back(req->value);
    }
    return true;
}

bool SimpleKvStore::Delete(const DeleteRequest* req, DeleteResponse* res) {
    // TODO (Part A, Step 1 and Step 2): Implement!

    std::lock_guard<std::mutex> lock(mtx);
    auto it = store.find(req->key);
    if(it == store.end())
    {
        return false;
    }
    auto& vec = it->second;
    std::string joint_val;
    for (auto& f : vec)
        joint_val += f;

    res->value = joint_val;
    store.erase(req->key);
    return true;
}

bool SimpleKvStore::MultiGet(const MultiGetRequest* req,
                             MultiGetResponse* res) {
    // TODO (Part A, Step 1 and Step 2): Implement!

    std::lock_guard<std::mutex> lock(mtx);
    res->values.clear();

    for(size_t i = 0; i < req->keys.size(); ++i)
    {
        auto val_it = store.find(req->keys[i]);
        if(val_it == store.end())
        {
            return false;
        }
        auto& vec = val_it->second;

        std::string joint_val;
        for (auto& f : vec)
            joint_val += f;

        res->values.push_back(joint_val);
    }

    return true;
}

bool SimpleKvStore::MultiPut(const MultiPutRequest* req, MultiPutResponse*) {
    // TODO (Part A, Step 1 and Step 2): Implement!
    if(req->keys.size() != req->values.size())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx);
    for(size_t i = 0; i < req->keys.size(); ++i)
    {
        auto key = req->keys[i];
        auto val = req->values[i];
        store[key] = { val };
    }

    return true;
}

std::vector<std::string> SimpleKvStore::AllKeys() {
    // TODO (Part A, Step 1 and Step 2): Implement!

    std::vector<std::string> allkeys;
    for(auto it = store.begin(); it != store.end(); ++it)
    {
        allkeys.push_back(it->first);
    }
    return allkeys;
}
