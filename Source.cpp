#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <stack>
#include <queue>
#include <fstream>
#include <algorithm>
#include <omp.h>
#include <climits> // For INT_MAX

#undef min
using namespace std;

// Константы и глобальные переменные
const int THREAD_COUNT = 4;  // Количество потоков
int global_minima[THREAD_COUNT];  // Для WinAPI
LONG global_min_interlocked = LONG_MAX;  // Для Interlocked
std::atomic<int> global_min_atomic{ LONG_MAX };  // Для std::atomic
int global_min_producer_consumer = INT_MAX;  // Для производителя-потребителя

// Структура для передачи данных в потоки WinAPI
struct ThreadData {
    const std::vector<int>* arr;
    int start;
    int end;
    int index;
};

void read_from_file(std::ifstream& file, std::vector<int>& arr) {
    int x;
    arr.clear();
    while (file >> x) {
        arr.push_back(x);
    }
    file.close();
}

// Задание 1: WinAPI - Создание потоков с помощью CreateThread
DWORD WINAPI FindMinWinAPI(LPVOID param) {
    ThreadData* data = (ThreadData*)param;
    int local_min = (*data->arr)[data->start];
    for (int i = data->start; i < data->end; ++i) {
        if ((*data->arr)[i] < local_min) local_min = (*data->arr)[i];
    }
    global_minima[data->index] = local_min;
    return 0;
}

int findMinWinAPI(const std::vector<int>& arr) {
    if (arr.empty()) return INT_MAX; // Handle empty array
    HANDLE threads[THREAD_COUNT];
    ThreadData thread_data[THREAD_COUNT];
    int chunk_size = arr.size() / THREAD_COUNT;
    // Создание потоков для поиска минимума
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_data[i].arr = &arr;
        thread_data[i].start = i * chunk_size;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? arr.size() : (i + 1) * chunk_size;
        thread_data[i].index = i;
        threads[i] = CreateThread(NULL, 0, FindMinWinAPI, &thread_data[i], 0, NULL);
    }
    WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, INFINITE);
    // Объединение локальных минимумов
    int global_min = global_minima[0];
    for (int i = 1; i < THREAD_COUNT; ++i) {
        global_min = std::min(global_min, global_minima[i]);
    }
    for (int i = 0; i < THREAD_COUNT; ++i) CloseHandle(threads[i]);
    return global_min;
}

// Задание 2: std::thread - Использование потоков STL
void FindMinThread(const std::vector<int>& arr, int start, int end, int& result) {
    int local_min = arr[start];
    for (int i = start; i < end; ++i) {
        if (arr[i] < local_min) local_min = arr[i];
    }
    result = local_min;
}

int findMinThread(const std::vector<int>& arr) {
    if (arr.empty()) return INT_MAX; // Handle empty array
    std::vector<std::thread> threads;
    std::vector<int> minima(THREAD_COUNT);
    int chunk_size = arr.size() / THREAD_COUNT;
    // Запуск потоков
    for (int i = 0; i < THREAD_COUNT; ++i) {
        int start = i * chunk_size;
        int end = (i == THREAD_COUNT - 1) ? arr.size() : (i + 1) * chunk_size;
        threads.emplace_back(FindMinThread, std::ref(arr), start, end, std::ref(minima[i]));
    }
    for (auto& t : threads) t.join();
    // Поиск глобального минимума
    int global_min = minima[0];
    for (int i = 1; i < THREAD_COUNT; ++i) {
        global_min = std::min(global_min, minima[i]);
    }
    return global_min;
}

// Задание 3: std::future - Асинхронные задачи
int FindMinFuture(const std::vector<int>& arr, int start, int end) {
    int local_min = arr[start];
    for (int i = start; i < end; ++i) {
        if (arr[i] < local_min) local_min = arr[i];
    }
    return local_min;
}

int findMinFuture(const std::vector<int>& arr) {
    if (arr.empty()) return INT_MAX; // Handle empty array
    std::vector<std::future<int>> futures;
    int chunk_size = arr.size() / THREAD_COUNT;
    // Запуск асинхронных задач
    for (int i = 0; i < THREAD_COUNT; ++i) {
        int start = i * chunk_size;
        int end = (i == THREAD_COUNT - 1) ? arr.size() : (i + 1) * chunk_size;
        futures.push_back(std::async(std::launch::async, FindMinFuture, std::ref(arr), start, end));
    }
    // Сбор результатов
    std::vector<int> minima(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        minima[i] = futures[i].get();
    }
    int global_min = minima[0];
    for (int i = 1; i < THREAD_COUNT; ++i) {
        global_min = std::min(global_min, minima[i]);
    }
    return global_min;
}

// Задание 4: Interlocked-функции
DWORD WINAPI FindMinInterlocked(LPVOID param) {
    ThreadData* data = (ThreadData*)param;
    int local_min = (*data->arr)[data->start];
    for (int i = data->start; i < data->end; ++i) {
        if ((*data->arr)[i] < local_min) local_min = (*data->arr)[i];
    }
    // Атомарное обновление минимума (Interlocked)
    LONG current_min = global_min_interlocked;
    while (local_min < current_min) {
        LONG old_min = InterlockedCompareExchange(&global_min_interlocked, local_min, current_min);
        if (old_min == current_min) break;
        current_min = old_min;
    }
    return 0;
}

void findMinInterlocked(const std::vector<int>& arr) {
    if (arr.empty()) return; // No action for empty array
    HANDLE threads[THREAD_COUNT];
    ThreadData thread_data[THREAD_COUNT];
    int chunk_size = arr.size() / THREAD_COUNT;
    global_min_interlocked = LONG_MAX;
    global_min_atomic = LONG_MAX;
    // Создание потоков
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_data[i].arr = &arr;
        thread_data[i].start = i * chunk_size;
        thread_data[i].end = (i == THREAD_COUNT - 1) ? arr.size() : (i + 1) * chunk_size;
        threads[i] = CreateThread(NULL, 0, FindMinInterlocked, &thread_data[i], 0, NULL);
    }
    WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, INFINITE);
    for (int i = 0; i < THREAD_COUNT; ++i) CloseHandle(threads[i]);
}

// Задание 5: Mutex, пул потоков, потокобезопасный стек
// Глобальные переменные для мьютекса
HANDLE hMutexPool;
int global_min_mutex_pool = INT_MAX;  // Для пула потоков с мьютексом

// Определяем структуру Task и класс ThreadSafeStack
struct Task {
    int start;
    int end;
};

class ThreadSafeStack {
private:
    std::stack<Task> tasks;
public:
    bool pop(Task& task) {
        WaitForSingleObject(hMutexPool, INFINITE);
        if (tasks.empty()) {
            ReleaseMutex(hMutexPool);
            return false;
        }
        task = tasks.top();
        tasks.pop();
        ReleaseMutex(hMutexPool);
        return true;
    }

    void push(const Task& task) {
        WaitForSingleObject(hMutexPool, INFINITE);
        tasks.push(task);
        ReleaseMutex(hMutexPool);
    }
};

// Глобальные переменные для Задания 5
ThreadSafeStack* taskStackPtr;
const std::vector<int>* arrPtrMutexPool;

DWORD WINAPI ThreadPoolWorker(LPVOID param) {
    Task task;
    // Обработка задач из стека
    while (taskStackPtr->pop(task)) {
        int local_min = (*arrPtrMutexPool)[task.start];
        for (int i = task.start; i < task.end; ++i) {
            if ((*arrPtrMutexPool)[i] < local_min) local_min = (*arrPtrMutexPool)[i];
        }
        // Синхронизация обновления минимума
        WaitForSingleObject(hMutexPool, INFINITE);
        if (local_min < global_min_mutex_pool) global_min_mutex_pool = local_min;
        ReleaseMutex(hMutexPool);
    }
    return 0;
}

int findMinMutexPool(const std::vector<int>& arr) {
    if (arr.empty()) return INT_MAX; // Handle empty array
    ThreadSafeStack taskStack;
    taskStackPtr = &taskStack;
    arrPtrMutexPool = &arr;
    global_min_mutex_pool = INT_MAX;

    hMutexPool = CreateMutex(NULL, FALSE, NULL);
    int chunk_size = arr.size() / THREAD_COUNT;
    // Создание задач для пула потоков
    for (int i = 0; i < THREAD_COUNT; ++i) {
        Task task;
        task.start = i * chunk_size;
        task.end = (i == THREAD_COUNT - 1) ? arr.size() : (i + 1) * chunk_size;
        taskStack.push(task);
    }

    HANDLE threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads[i] = CreateThread(NULL, 0, ThreadPoolWorker, NULL, 0, NULL);
    }
    WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, INFINITE);
    for (int i = 0; i < THREAD_COUNT; ++i) CloseHandle(threads[i]);
    CloseHandle(hMutexPool);
    return global_min_mutex_pool;
}

// Задание 6: Producer-Consumer
int findMinProducerConsumer(const std::vector<int>& arr) {
    if (arr.empty()) return INT_MAX; // Handle empty array

    queue<int> taskQueue; // Queue of indices to process
    mutex mtx;
    condition_variable cv;
    vector<int> localMins; // Store local minimums from consumers
    bool done = false;

    // Producer: adds indices to the queue
    auto producer = [&]() {
        for (size_t i = 0; i < arr.size(); ++i) {
            {
                lock_guard<mutex> lock(mtx);
                taskQueue.push(i); // Push index of element to process
            }
            cv.notify_one();
        }
        {
            lock_guard<mutex> lock(mtx);
            done = true; // Signal completion
        }
        cv.notify_all();
        };

    // Consumer: processes elements to find local minimum
    auto consumer = [&]() {
        int localMin = INT_MAX;
        while (true) {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [&]() { return !taskQueue.empty() || done; });

            if (taskQueue.empty() && done) {
                break; // Exit when queue is empty and producer is done
            }

            if (!taskQueue.empty()) {
                int index = taskQueue.front();
                taskQueue.pop();
                lock.unlock(); // Unlock before processing
                if (arr[index] < localMin) {
                    localMin = arr[index];
                }
            }
        }
        {
            lock_guard<mutex> lock(mtx);
            if (localMin != INT_MAX) {
                localMins.push_back(localMin); // Store local minimum
            }
        }
        };

    // Start threads
    thread producer_thread(producer);
    thread consumer_thread_1(consumer), consumer_thread_2(consumer);

    // Wait for threads to finish
    producer_thread.join();
    consumer_thread_1.join();
    consumer_thread_2.join();

    // Find global minimum from local minimums
    int globalMin = INT_MAX;
    for (int min : localMins) {
        if (min < globalMin) {
            globalMin = min;
        }
    }
    // If no minimum was found, scan the array
    if (globalMin == INT_MAX) {
        for (int val : arr) {
            if (val < globalMin) {
                globalMin = val;
            }
        }
    }

    return globalMin;
}

// Задание 7: OpenMP
int findMinOpenMP(const std::vector<int>& arr) {
    if (arr.empty()) return INT_MAX; // Handle empty array

    int globalMin = INT_MAX;

#pragma omp parallel
    {
        int localMin = INT_MAX;

#pragma omp for
        for (size_t i = 0; i < arr.size(); ++i) {
            if (arr[i] < localMin) {
                localMin = arr[i];
            }
        }

#pragma omp critical
        {
            if (localMin < globalMin) {
                globalMin = localMin;
            }
        }
    }

    return globalMin;
}

int main() {
    std::ifstream file("data.txt");
    std::vector<int> arr;
    read_from_file(file, arr);
    std::cout << "Read " << arr.size() << " elements from file." << std::endl;

    // Вызов всех реализаций
    std::cout << "Minimum element (WinAPI): " << findMinWinAPI(arr) << std::endl;
    std::cout << "Minimum element (std::thread): " << findMinThread(arr) << std::endl;
    std::cout << "Minimum element (std::future): " << findMinFuture(arr) << std::endl;
    findMinInterlocked(arr);
    std::cout << "Minimum element (Interlocked): " << global_min_interlocked << std::endl;
    std::cout << "Minimum element (Mutex Thread Pool): " << findMinMutexPool(arr) << std::endl;
    std::cout << "Minimum element (Producer-Consumer): " << findMinProducerConsumer(arr) << std::endl;
    std::cout << "Minimum element (OpenMP): " << findMinOpenMP(arr) << std::endl;

    return 0;
}