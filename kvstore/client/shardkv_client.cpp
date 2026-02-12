#include "shardkv_client.hpp"

std::optional<std::string> ShardKvClient::Get(const std::string& key) {
    // Query shardcontroller for config
    auto config = this->Query();
    if (!config) return std::nullopt;

    // find responsible server in config
    std::optional<std::string> server = config->get_server(key);
    if (!server) return std::nullopt;

    return SimpleClient{*server}.Get(key);
}

bool ShardKvClient::Put(const std::string& key, const std::string& value) {
    // Query shardcontroller for config
    auto config = this->Query();
    if (!config) return false;

    // find responsible server in config, then make Put request
    std::optional<std::string> server = config->get_server(key);
    if (!server) return false;
    return SimpleClient{*server}.Put(key, value);
}

bool ShardKvClient::Append(const std::string& key, const std::string& value) {
    // Query shardcontroller for config
    auto config = this->Query();
    if (!config) return false;

    // find responsible server in config, then make Append request
    std::optional<std::string> server = config->get_server(key);
    if (!server) return false;
    return SimpleClient{*server}.Append(key, value);
}

std::optional<std::string> ShardKvClient::Delete(const std::string& key) {
    // Query shardcontroller for config
    auto config = this->Query();
    if (!config) return std::nullopt;

    // find responsible server in config, then make Delete request
    std::optional<std::string> server = config->get_server(key);
    if (!server) return std::nullopt;
    return SimpleClient{*server}.Delete(key);
}

std::optional<std::vector<std::string>> ShardKvClient::MultiGet(
    const std::vector<std::string>& keys) {
    // TODO (Part B, Step 3): Implement!
    auto config = this->Query();
    if (!config) return std::nullopt;

    // Group keys by server
    std::map<std::string, std::vector<size_t>> server_to_indices;
    for(size_t i = 0; i < keys.size(); ++i)
    {
        auto server = config->get_server(keys[i]);
        if (!server) return std::nullopt;
        server_to_indices[*server].push_back(i);
    }

    // // Prepare output (same order as input)
    std::vector<std::string> values;
    values.reserve(keys.size());

    // For each server, issue MultiGet
    for (auto& [server, indices] : server_to_indices) {
        std::vector<std::string> server_keys;
        server_keys.reserve(indices.size());

        for (auto idx : indices) {
            server_keys.push_back(keys[idx]);
        }

        // Send MultiGet to that server
        auto res = SimpleClient{server}.MultiGet(server_keys);
        if (!res || res->size() != indices.size())
            return std::nullopt;
        for (size_t i = 0; i < indices.size(); ++i) {
            values[indices[i]] = (*res)[i];
        }
    }

    return values;
}

bool ShardKvClient::MultiPut(const std::vector<std::string>& keys,
                             const std::vector<std::string>& values) {
    // TODO (Part B, Step 3): Implement!

    auto config = this->Query();
    if (!config) return false;

    // Group keys by server
    std::map<std::string, std::vector<size_t>> server_to_indices;
    for(size_t i = 0; i < keys.size(); ++i)
    {
        auto server = config->get_server(keys[i]);
        if (!server) return false;
        server_to_indices[*server].push_back(i);
    }
    //
    // For each server, issue MultiGet
    for (auto& [server, indices] : server_to_indices) {
        std::vector<std::string> server_values;
        std::vector<std::string> server_keys;
        server_keys.reserve(indices.size());
        server_values.reserve(indices.size());

        for (auto idx : indices) {
            server_keys.push_back(keys[idx]);
            server_values.push_back(values[idx]);
        }

        // Send MultiGet to that server
        auto res = SimpleClient{server}.MultiPut(server_keys, server_values);
        if (!res)
            return false;
    }

    return true;
}

// Shardcontroller functions
std::optional<ShardControllerConfig> ShardKvClient::Query() {
    QueryRequest req;
    if (!this->shardcontroller_conn->send_request(req)) return std::nullopt;

    std::optional<Response> res = this->shardcontroller_conn->recv_response();
    if (!res) return std::nullopt;
    if (auto* query_res = std::get_if<QueryResponse>(&*res)) {
        return query_res->config;
    }

    return std::nullopt;
}

bool ShardKvClient::Move(const std::string& dest_server,
                         const std::vector<Shard>& shards) {
    MoveRequest req{dest_server, shards};
    if (!this->shardcontroller_conn->send_request(req)) return false;

    std::optional<Response> res = this->shardcontroller_conn->recv_response();
    if (!res) return false;
    if (auto* move_res = std::get_if<MoveResponse>(&*res)) {
        return true;
    }

    return false;
}
