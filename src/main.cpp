#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <pthread.h>
#include <sched.h>
#include <cassert>
#include <map>
#include <array>
#include <math.h>
#include <sstream>

// Set this to the number of CPUs that you have available
constexpr std::array<int, 8> CPU_IDS = { 0, 1, 2, 3, 4, 5, 6, 7 };

struct mylock1
{
    std::atomic<bool> locked;

    mylock1() : locked(false) {}
    void lock()
    {
        bool f = false;
        while (!locked.compare_exchange_weak(f, true)) {
            f = false;
        }
    }

    void unlock() { locked.store(false); }
};

struct mylock2
{
    std::atomic<bool> locked;
    mylock2() : locked(false) {}

    void lock()
    {
        while (true) {
            while (locked.load()) {
            }
            bool f = false;
            if (locked.compare_exchange_weak(f, true)) {
                break;
            }
        }
    }

    void unlock()
    {
        // while (true) {
        //     while (!locked.load()) {
        //     }
        //     bool t = true;
        //     if (locked.compare_exchange_weak(t, false)) {
        //         break;
        //     }
        // }
        locked.store(false);
    }
};

void write_log(std::string name, std::vector<int> const &log)
{
    std::ofstream file;
    file.open(name);
    for (size_t i = 0; i < log.size(); ++i) {
        file << log[i] << (i == log.size() - 1 ? "" : ",");
    }
    file.close();
}

void set_high_priority()
{
    if (nice(-19) == -1) {
        std::cout << "WARNING: unable to set priority, you might need to use sudo" << std::endl;
    }
}

void pin(int thid)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thid, &cpuset);
    pthread_t th = pthread_self();
    std::stringstream ss;
    if (pthread_setaffinity_np(th, sizeof(cpu_set_t), &cpuset) != 0) {
        ss << "WARNING: could not pin thread=" << th << " to core=" << thid << "\n";
    }
    std::cout << ss.str();
}

/// Compute the clustering stats
std::map<std::pair<int, int>, int> find_clusters(std::vector<int> const &log)
{
    std::map<std::pair<int, int>, int> count;
    for (int i = 0; i < (int)log.size() - 1; ++i) {
        std::pair<int, int> p = { log[i], log[i + 1] };
        count[p]++;
    }
    return count;
}

void spin(long n)
{
    volatile long x = 0;
    for (long i = 0; i < n; ++i) {
        x = i;
    }
    (void)x;
}

/// @brief Benchmark a lock (`Lock` should have a default constructor, a
/// `lock()` and an `unlock()` method).
/// @tparam Lock The lock class to benchmark
/// @param iters
/// @param hold_iters
template <class Lock> void bm_lock(int iters, int hold_iters)
{
    Lock mtx;
    std::vector<std::thread> ths;
    std::vector<int> log;
    std::atomic<bool> wait = true;
    std::atomic<int> ready = 0;
    int count = 0, expected_count = (int)CPU_IDS.size() * iters;
    log.reserve(expected_count); // we must reserve, memmove() flushes cache
    for (auto const thid : CPU_IDS) {
        ths.emplace_back(std::thread([&, thid, iters]() {
            pin(thid);
            ready++;
            while (ready < (int)CPU_IDS.size()) {
            }
            for (int i = 0; i < iters; ++i) {
                mtx.lock();
                count++;
                log.push_back(thid);
                spin(hold_iters);
                mtx.unlock();
            }
        }));
    }
    // Time it
    auto start = std::chrono::system_clock::now();
    for (auto &th : ths) {
        th.join();
    }
    auto end = std::chrono::system_clock::now();
    assert(count == expected_count);
    auto const duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << typeid(Lock).name() << "\n\titers      : " << iters
              << "\n\thold_iters : " << hold_iters << "\n\ttime       : " << duration.count() << "us"
              << "\n\tclusters   : ";
    // Compute clusters
    auto const clusters = find_clusters(log);
    double total_count = expected_count - 1;
    int col = 0; 
    for (auto const &[ab, count] : clusters) {
    	int percentage = (10000.0 * (double)count / total_count) + 0.5;
    	if (col == 0) std::cout << "\n\t\t";
    	std::stringstream ss;
        ss << "(" << ab.first << "," << ab.second << "): " << ((double)percentage / 100.0) << ", " << count;
        size_t rem = 40 - ss.str().size();
        for (int i = 0; i < rem; ++i) {
        	ss << " ";
        }
        std::cout << ss.str();
        col = (col + 1) % 4;
    }
    std::cout << std::endl;
    write_log(typeid(Lock).name(), log);
}

int main()
{
    set_high_priority();

    bm_lock<mylock1>(50e3, 0); // should be faster
    bm_lock<mylock2>(50e3, 0); // should 
    bm_lock<mylock1>(50e3, 1e2);
    bm_lock<mylock2>(50e3, 1e2);
    bm_lock<mylock1>(50e3, 1e4);
    bm_lock<mylock2>(50e3, 1e4);

    return 0;
}
