#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>

namespace prll {
#define MAXNTHREADS (size_t)50

class Parallel {
  public:
    Parallel();

    template<typename Callable, typename... Args>
    void add(Callable &&f, Args &&... args) {
        auto task = std::bind(std::forward<Callable>(f),
                              std::forward<Args>(args)...);

        if (_max_threads == 0) {
            task();
        } else {
            std::unique_lock<std::mutex> lck(_task_mutex);

            if (_exit) {
                return;
            }

            _tasks.push(task);

            lck.unlock();
            _in_thread.notify_all();
            _in_balance.notify_one();
        }
    }

    void wait();

    void join();

    void stop();

    void set_max_threads(size_t max_threads);

    [[nodiscard]] size_t get_count_threads() const;

    ~Parallel();

  private:

    class Thread {
      public:
        Thread() = delete;

        Thread(std::mutex &thread_mutex,
               std::queue<std::function<void()>> &task,
               std::condition_variable &threads);

        Thread(Thread &&) noexcept = delete;

        void join();

        void wait();

        void force_join();

        ~Thread();

      private:

        void _main();

        std::mutex &_task_mutex;
        std::condition_variable &_in_thread;
        std::queue<std::function<void(void)>> &_tasks;

        bool _end;
        std::thread _main_thread;
        std::condition_variable _wait;
        std::atomic<long> _have_proccess;
    };

    void _balance();

    std::mutex _task_mutex;
    std::condition_variable _in_thread;
    std::queue<std::function<void(void)>> _tasks;

    std::mutex _main_mutex;
    std::condition_variable _in_balance;
    std::vector<std::unique_ptr<Thread>> _threads;
    std::thread _main_thread;

    bool _exit;
    size_t _max_threads;
};
}