#include <iostream>

#include <chrono>
#include <string>
#include <pqxx/pqxx>
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>

typedef std::pair<const char *, const char *> Statement;

#define retries 5

class ConnectionPool
{
public:
    class ConnectionGuard
    {
    public:
        ConnectionGuard(std::shared_ptr<pqxx::connection> conn, ConnectionPool &pool)
            : conn_(std::move(conn)), pool_(pool) {}

        ~ConnectionGuard()
        {
            std::lock_guard lock(pool_.mutex_);
            pool_.idle_connections_.push(conn_);
            pool_.condition_.notify_one();
            --pool_.active_count_;
        }

        pqxx::connection &operator*() const
        {
            return *conn_;
        }

        pqxx::connection *operator->() const
        {
            return conn_.get();
        }

    private:
        std::shared_ptr<pqxx::connection> conn_;
        ConnectionPool &pool_;
    };

    size_t connections;
    std::chrono::milliseconds timeout;
    std::queue<std::shared_ptr<pqxx::connection>> idle_connections_;
    std::atomic_size_t active_count_{0};
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<Statement> statements;
    const char *conn_str_;

public:
    //throw pqxx::broken_connection if no connection is established
    ConnectionPool(const char *conn_str, size_t connections, std::chrono::milliseconds timeout,
                   const std::vector<Statement> &statements) : conn_str_(conn_str),
                                                               connections(connections), timeout(timeout), statements(statements)
    {
        size_t failed = 0;
        for (size_t i = 0; i < connections; ++i)
        {
            for (int i = 0; i < retries; i++)
            {
                try
                {
                    idle_connections_.push(create_connection());
                    break;
                }
                catch (const pqxx::broken_connection &e)
                {
                    if (i == retries - 1)
                        ++failed;
                    continue;
                }
            }
        }
        if (failed == connections)
        {
            throw pqxx::broken_connection("Couldnt connect to database, while initiailzing connections");
        }
    };
    
    //throw pqxx::broken_connection if no connection got free in give time frame
    ConnectionGuard get_connection()
    {
        std::unique_lock lock(mutex_);
        if (!condition_.wait_for(lock, timeout, [this]
                                 { return !idle_connections_.empty(); }))
        {
            throw std::runtime_error("Connection pool timeout");
        }

        auto conn = idle_connections_.front();
        idle_connections_.pop();

        if (conn && conn->is_open())
        {
            ++active_count_;
            return ConnectionGuard(conn, *this);
        }
        protocol_con_failed();
        throw std::runtime_error("Connection failed");
    }
    
    //delete all the connections
    void drain()
    {
        std::lock_guard lock(mutex_);
        while (idle_connections_.size() + active_count_)
        {
            idle_connections_.pop();
        }
        active_count_ = 0;
    }

    //tries to increase connection by one, if couldnt connect return 0;
    bool increase_connection()
    {

        try
        {
            auto conn = create_connection();
            std::unique_lock lock(mutex_);
            idle_connections_.push(conn);
            ++connections;
            return 1;
        }
        catch (pqxx::broken_connection &e)
        {
            return 0;
        }
    }
    
    // idk why i made it that way but return false if couldnt established the conn wait for 10sec before 
    // returning false
    bool decrease_connection()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait for 10 seconds or until the condition is met (e.g., resource available)
        if (!condition_.wait_for(lock, std::chrono::seconds(10), [this]
                                 { return !idle_connections_.empty(); }))
        {
            return false; // Timeout occurred
        }
        idle_connections_.pop();
        --connections;
        return true;
    }

    // return current number of connections
    size_t get_current_connections()
    {
        return connections;
    }

private:
    // create connections throw pqxx:broken_connection if couldnt
    std::shared_ptr<pqxx::connection> create_connection()
    {
        auto conn = std::make_shared<pqxx::connection>(conn_str_);
        if (!conn->is_open())
            throw pqxx::broken_connection("Connection failed");
        for (const auto &[name, sql] : statements)
        {
            conn->prepare(name, sql);
        }
        return conn;
    }
    // if connection got disconnected while being in idle state, 
    // it just decrease the number of connections
    void protocol_con_failed()
    {
        connections--;
    }

};
