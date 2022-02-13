#pragma once

#ifdef _WIN32 // Windows NT

#include <WinSock2.h>
#include <mstcpip.h>
#include <ws2tcpip.h>

#else // *nix

#define SD_BOTH 0

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>

#endif


#include <cstdint>
#include <cstring>
#include <cinttypes>
#include <malloc.h>

#include <queue>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace bstcp {

constexpr uint32_t localhost = 0x0100007f; //127.0.0.1


#ifdef _WIN32 // Windows NT

typedef int sock_len_t;
typedef SOCKADDR_IN socket_addr_in;
typedef SOCKET socket_t;
typedef u_long ka_prop_t;

#else // POSIX

typedef socklen_t sock_len_t;
typedef struct sockaddr_in socket_addr_in;
typedef int socket_t;
typedef int ka_prop_t;

#endif

template <typename T>
using uniq_ptr = std::unique_ptr<T>;

enum class SocketType : uint8_t {
    client_socket       = 0,
    server_socket       = 1,
    blocking_socket     = 2,
    nonblocking_socket  = 4,
};

enum class SocketStatus : uint8_t {
    connected               = 0,
    err_socket_init         = 1,
    err_socket_bind         = 2,
    err_socket_connect      = 3,
    disconnected            = 4,
    err_socket_type         = 5,
    err_socket_listening    = 6,
};

int hostname_to_ip(const char *hostname, socket_addr_in *addr);

class ThreadPool {
    std::vector<std::thread> thread_pool;
    std::queue<std::function<void()>> job_queue;
    std::mutex queue_mtx;
    std::condition_variable condition;
    std::atomic<bool> pool_terminated = false;

    void setupThreadPool(size_t thread_count) {
        thread_pool.clear();
        for (size_t i = 0; i < thread_count; ++i)
            thread_pool.emplace_back(&ThreadPool::workerLoop, this);
    }

    void workerLoop() {
        std::function<void()> job;
        while (!pool_terminated) {
            {
                std::unique_lock lock(queue_mtx);
                condition.wait(lock, [this]() {
                    return !job_queue.empty() || pool_terminated;
                });
                if (pool_terminated) return;
                job = job_queue.front();
                job_queue.pop();
            }
            job();
        }
    }

  public:
    ThreadPool(size_t thread_count = std::thread::hardware_concurrency()) {
        setupThreadPool(thread_count);
    }

    ~ThreadPool() {
        pool_terminated = true;
        join();
    }

    template<typename F>
    void addJob(F job) {
        if (pool_terminated) return;
        {
            std::unique_lock lock(queue_mtx);
            job_queue.push(std::function<void()>(job));
        }
        condition.notify_one();
    }

    template<typename F, typename... Arg>
    void addJob(const F &job, const Arg &... args) {
        addJob([job, args...] { job(args...); });
    }

    void join() {
        for (auto &thread: thread_pool) thread.join();
    }

    [[nodiscard]] size_t getThreadCount() const { return thread_pool.size(); }

    void dropUnstartedJobs() {
        pool_terminated = true;
        join();
        pool_terminated = false;
        // Clear jobs in queue
        std::queue<std::function<void()>> empty;
        std::swap(job_queue, empty);
        // reset thread pool
        setupThreadPool(thread_pool.size());
    }

    void stop() {
        pool_terminated = true;
        join();
    }

    void start(size_t thread_count = std::thread::hardware_concurrency()) {
        if (!pool_terminated) return;
        pool_terminated = false;
        setupThreadPool(thread_count);
    }

};

typedef std::vector<uint8_t> tcp_data_t;

typedef SocketStatus status;

class IReceivable {
  public:
    virtual ~IReceivable() = default;

    virtual bool recv_from(void *buffer, int size) = 0;
};

class ISendable {
  public:
    virtual ~ISendable() = default;

    virtual bool send_to(const void *buffer, size_t size) const = 0;
};

class ISendRecvable : public IReceivable, ISendable {
  public:
    ~ISendRecvable() override = default;
};

class IDisconnectable {
  public:
    virtual ~IDisconnectable() = default;

    virtual status disconnect() = 0;
};

class ISocket : public ISendRecvable, IDisconnectable {
  public:
    ~ISocket() override = default;

    [[nodiscard]] virtual status get_status() const = 0;

    [[nodiscard]] virtual uint32_t get_host() const = 0;

    [[nodiscard]] virtual uint16_t get_port() const = 0;

    [[nodiscard]] virtual SocketType get_type() const = 0;
};

#ifdef _WIN32 // Windows NT

namespace {
class WinSocketIniter {
    WSADATA w_data;
    public:
    WinSocketIniter() {
        w_data = WSADATA();
        WSAStartup(MAKEWORD(2, 2), &w_data);
    }

    ~WinSocketIniter() {
        WSACleanup();
    }
};
[[maybe_unused]] static inline WinSocketIniter _winsock_initer;
}

#endif

}