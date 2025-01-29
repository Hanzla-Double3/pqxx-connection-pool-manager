#include "ConnectionPool.hpp"
#include <unordered_map>

// Singleton class for managing mutliple connection pools
class ConnectionPoolManager
{
    std::unordered_map<std::string, ConnectionPool> pools_;
    std::mutex mutex_;

public:
    static ConnectionPoolManager &instance();

    bool registerPool(const char *conn_str_, const std::string &name, size_t numbr_conns,
                      std::chrono::milliseconds timeout, const std::vector<Statement> &statements);

    ConnectionPool &get_pool(const std::string &name);

    void shutdown();
};

ConnectionPoolManager &ConnectionPoolManager::instance()
{
    static ConnectionPoolManager instance;
    return instance;
}

inline bool ConnectionPoolManager::registerPool(const char *conn_str_, const std::string &name, size_t numbr_conns, std::chrono::milliseconds timeout, const std::vector<Statement> &statements)
{
    try{
        std::lock_guard lock(mutex_);
        pools_.try_emplace(name, conn_str_, numbr_conns, timeout, statements);
    } catch (pqxx::broken_connection &e){
        return 0;
    }
    return 1;
}

// throw std::out_of_range if pool not present
inline ConnectionPool &ConnectionPoolManager::get_pool(const std::string &name)
{
    std::lock_guard lock(mutex_);
    return pools_.at(name);
}

//just drain all the connection pools
inline void ConnectionPoolManager::shutdown()
{
    std::lock_guard lock(mutex_);
    for (auto &[name, pool] : pools_)
    {
        pool.drain();
    }
    pools_.clear();
}
