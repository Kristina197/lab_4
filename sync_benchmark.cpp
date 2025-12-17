#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <condition_variable>

using namespace std;

// Реализация семафора через condition_variable
class CountingSemaphore {
private:
    mutex mtx;
    condition_variable cv;
    int count;
    
public:
    CountingSemaphore(int initial) : count(initial) {}
    
    void acquire() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]() { return count > 0; });
        count--;
    }
    
    void release() {
        unique_lock<mutex> lock(mtx);
        count++;
        cv.notify_one();
    }
};

// Реализация барьера
class SimpleBarrier {
private:
    mutex mtx;
    condition_variable cv;
    int count;
    int reset_value;
    
public:
    SimpleBarrier(int n) : count(n), reset_value(n) {}
    
    void wait() {
        unique_lock<mutex> lock(mtx);
        if (--count == 0) {
            count = reset_value;
            cv.notify_all();
        } else {
            cv.wait(lock);
        }
    }
};

// Реализация SpinWait 
class SpinWait {
private:
    atomic<bool> locked{false};
    
public:
    void lock() {
        while (true) {
            bool expected = false;
            if (locked.compare_exchange_weak(expected, true)) {
                return;
            }
            
            for (int i = 0; i < 100; i++) {
                // Пауза процессора (команда pause на x86)
                asm volatile("pause" ::: "memory");
                
                // Проверяем снова
                if (!locked.load()) {
                    continue;
                }
            }
            
            this_thread::yield();
        }
    }
    
    void unlock() {
        locked.store(false);
    }
    
    bool try_lock() {
        bool expected = false;
        return locked.compare_exchange_weak(expected, true);
    }
};

class SyncPrimitivesTest {
private:
    vector<thread> threads;
    mutex mtx;
    CountingSemaphore sem{3};
    SimpleBarrier barr{4};
    
    atomic<bool> spinLock{false}; 
    SpinWait spinWait;             
    
    condition_variable cv;
    bool monitorFlag = false;
    random_device rd;
    mt19937 gen;
    
    char get_random_char() {
        uniform_int_distribution<> dist(65, 90);
        return static_cast<char>(dist(gen));
    }
    
    char get_random_digit() {
        uniform_int_distribution<> dist(48, 57);
        return static_cast<char>(dist(gen));
    }
    
    char get_random_special() {
        uniform_int_distribution<> dist(33, 47);
        return static_cast<char>(dist(gen));
    }
    
    // 1. Mutex
    void worker_mutex(int id) {
        {
            lock_guard<mutex> lock(mtx);
            cout << "Поток " << id << " (Mutex): " << get_random_char() << endl;
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    
    // 2. Semaphore
    void worker_semaphore(int id) {
        sem.acquire();
        cout << "Поток " << id << " (Semaphore): " << get_random_char() << endl;
        this_thread::sleep_for(chrono::milliseconds(100));
        sem.release();
    }
    
    // 3. Barrier
    void worker_barrier(int id) {
        cout << "Поток " << id << " (Barrier) готовится: " << get_random_digit() << endl;
        barr.wait();
        cout << "Поток " << id << " (Barrier) продолжил: " << get_random_digit() << endl;
    }
    
    // 4. Простой SpinLock (активное ожидание без пауз)
    void worker_spinlock(int id) {
        // Активное ожидание (busy waiting)
        while (spinLock.exchange(true)) {
            // Пустой цикл - тратит процессорное время
        }
        cout << "Поток " << id << " (SpinLock): " << get_random_special() << endl;
        this_thread::sleep_for(chrono::milliseconds(50));
        spinLock.store(false);
    }
    
    // 5. SpinWait (активное ожидание с оптимизациями)
    void worker_spinwait(int id) {
        spinWait.lock();  // Использует паузы и yield
        cout << "Поток " << id << " (SpinWait): " << get_random_special() << endl;
        this_thread::sleep_for(chrono::milliseconds(50));
        spinWait.unlock();
    }
    
    // 6. Monitor (condition variable)
    void worker_monitor(int id) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]() { return !monitorFlag; });
        monitorFlag = true;
        cout << "Поток " << id << " (Monitor): " << get_random_char() << endl;
        this_thread::sleep_for(chrono::milliseconds(100));
        monitorFlag = false;
        cv.notify_one();
    }

public:
    SyncPrimitivesTest() : gen(rd()) {}
    
    void run_comparison() {
        
        // 1. Mutex
        cout << "\n1. Mutex (стандартная блокировка):\n";
        auto start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 4; i++) {
            threads.emplace_back(&SyncPrimitivesTest::worker_mutex, this, i);
        }
        for (auto& t : threads) t.join();
        threads.clear();
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Время: " << duration.count() << " мс\n";
        
        // 2. Semaphore
        cout << "\n2. Semaphore (ограничение потоков):\n";
        start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 4; i++) {
            threads.emplace_back(&SyncPrimitivesTest::worker_semaphore, this, i);
        }
        for (auto& t : threads) t.join();
        threads.clear();
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Время: " << duration.count() << " мс\n";
        
        // 3. Barrier
        cout << "\n3. Barrier (синхронизация в точке):\n";
        start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 4; i++) {
            threads.emplace_back(&SyncPrimitivesTest::worker_barrier, this, i);
        }
        for (auto& t : threads) t.join();
        threads.clear();
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Время: " << duration.count() << " мс\n";
        
        // 4. SpinLock (простой)
        cout << "\n4. SpinLock (активное ожидание):\n";
        start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 4; i++) {
            threads.emplace_back(&SyncPrimitivesTest::worker_spinlock, this, i);
        }
        for (auto& t : threads) t.join();
        threads.clear();
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Время: " << duration.count() << " мс\n";
        
        // 5. SpinWait (умный)
        cout << "\n5. SpinWait (активное ожидание с паузами):\n";
        start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 4; i++) {
            threads.emplace_back(&SyncPrimitivesTest::worker_spinwait, this, i);
        }
        for (auto& t : threads) t.join();
        threads.clear();
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Время: " << duration.count() << " мс\n";
        
        // 6. Monitor
        cout << "\n6. Monitor (condition variable):\n";
        start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 4; i++) {
            threads.emplace_back(&SyncPrimitivesTest::worker_monitor, this, i);
        }
        for (auto& t : threads) t.join();
        threads.clear();
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "Время: " << duration.count() << " мс\n";
        
    }
};

int main() {
    
    SyncPrimitivesTest test;
    test.run_comparison();
    
    return 0;
}