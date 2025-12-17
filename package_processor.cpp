#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <string>
#include <regex>
#include <algorithm>
#include <future>
#include <unordered_map>

using namespace std;

struct Package {
    string code;
    string city;
    string recipient;
    
    Package(string c, string ci, string r) : code(c), city(ci), recipient(r) {}
};

class PackageProcessor {
private:
    vector<Package> packages;
    mutex resultMutex;
    
    void generate_test_data(size_t size) {
        vector<string> cities = {"Москва", "Санкт-Петербург", "Новосибирск", "Екатеринбург"};
        vector<string> surnames = {"Иванов", "Петров", "Сидоров", "Смирнов", "Кузнецов"};
        vector<string> names = {"Алексей", "Дмитрий", "Сергей", "Андрей", "Михаил"};
        
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<size_t> city_dist(0, cities.size() - 1);
        uniform_int_distribution<size_t> surname_dist(0, surnames.size() - 1);
        uniform_int_distribution<size_t> name_dist(0, names.size() - 1);
        uniform_int_distribution<int> code_dist(1000, 9999);
        
        packages.reserve(size);
        for (size_t i = 0; i < size; i++) {
            string code = "PKG-" + to_string(code_dist(gen));
            string city = cities[city_dist(gen)];
            string recipient = surnames[surname_dist(gen)] + " " + 
                               names[name_dist(gen)] + " " + 
                               names[name_dist(gen)] + "ович";
            packages.emplace_back(code, city, recipient);
        }
    }
    
    void process_chunk(size_t start, size_t end, const regex& pattern, 
                       vector<string>& results) {
        vector<string> local_results;
        local_results.reserve((end - start) / 10);
        
        for (size_t i = start; i < end; i++) {
            if (regex_match(packages[i].code, pattern)) {
                local_results.push_back(packages[i].recipient);
            }
        }
        
        if (!local_results.empty()) {
            lock_guard<mutex> lock(resultMutex);
            results.insert(results.end(), local_results.begin(), local_results.end());
        }
    }
    
    vector<string> process_chunk_async(size_t start, size_t end, const regex& pattern) {
        vector<string> local_results;
        local_results.reserve((end - start) / 10);
        
        for (size_t i = start; i < end; i++) {
            if (regex_match(packages[i].code, pattern)) {
                local_results.push_back(packages[i].recipient);
            }
        }
        
        return local_results;
    }

public:
    PackageProcessor(size_t size = 100000) {
        cout << "Генерация тестовых данных (" << size << " записей)..." << endl;
        generate_test_data(size);
        cout << "Данные сгенерированы успешно" << endl;
    }
    
    pair<vector<string>, long long> process_single(const string& pattern_str) {
        cout << "\nОднопоточная обработка:" << endl;
        
        auto start = chrono::high_resolution_clock::now();
        
        vector<string> results;
        results.reserve(packages.size() / 10);
        regex pattern(pattern_str);
        
        for (const auto& pkg : packages) {
            if (regex_match(pkg.code, pattern)) {
                results.push_back(pkg.recipient);
            }
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        
        return {results, duration.count()};
    }
    
    pair<vector<string>, long long> process_multi(const string& pattern_str, int num_threads) {
        cout << "\nМногопоточная обработка (" << num_threads << " потоков):" << endl;
        
        // Автоматическая корректировка числа потоков для малых данных
        if (packages.size() < 1000) {
            num_threads = 1;
            cout << "Внимание: данных меньше 1000, используется однопоточный режим" << endl;
        } else if (packages.size() / static_cast<size_t>(num_threads) < 100) {
            num_threads = max(1, static_cast<int>(packages.size() / 100));
            cout << "Внимание: скорректировано число потоков на " << num_threads 
                 << " для оптимальной загрузки" << endl;
        }
        
        auto start = chrono::high_resolution_clock::now();
        
        vector<string> results;
        regex pattern(pattern_str);
        vector<thread> threads;
        
        // Резервируем память заранее
        results.reserve(packages.size() / 10);
        
        size_t chunk_size = packages.size() / static_cast<size_t>(num_threads);
        size_t remainder = packages.size() % static_cast<size_t>(num_threads);
        
        size_t start_idx = 0;
        
        for (int i = 0; i < num_threads; i++) {
            size_t end_idx = start_idx + chunk_size + (static_cast<size_t>(i) < remainder ? 1 : 0);
            threads.emplace_back(&PackageProcessor::process_chunk, this, 
                                 start_idx, end_idx, cref(pattern), ref(results));
            start_idx = end_idx;
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        
        return {results, duration.count()};
    }
    
    pair<vector<string>, long long> process_multi_async(const string& pattern_str, int num_threads) {
        cout << "\nМногопоточная обработка (async, " << num_threads << " потоков):" << endl;
        
        // Автоматическая корректировка
        if (packages.size() < 1000) {
            num_threads = 1;
        } else if (packages.size() / static_cast<size_t>(num_threads) < 100) {
            num_threads = max(1, static_cast<int>(packages.size() / 100));
        }
        
        auto start = chrono::high_resolution_clock::now();
        
        vector<string> results;
        regex pattern(pattern_str);
        vector<future<vector<string>>> futures;
        
        size_t chunk_size = packages.size() / static_cast<size_t>(num_threads);
        size_t remainder = packages.size() % static_cast<size_t>(num_threads);
        
        size_t start_idx = 0;
        
        for (int i = 0; i < num_threads; i++) {
            size_t end_idx = start_idx + chunk_size + (static_cast<size_t>(i) < remainder ? 1 : 0);
            futures.push_back(async(launch::async, 
                &PackageProcessor::process_chunk_async, this, 
                start_idx, end_idx, cref(pattern)));
            start_idx = end_idx;
        }
        
        // Собираем результаты
        results.reserve(packages.size() / 10);
        for (auto& fut : futures) {
            auto chunk_result = fut.get();
            results.insert(results.end(), chunk_result.begin(), chunk_result.end());
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        
        return {results, duration.count()};
    }
    
    void print_sample(size_t count = 5) {
        cout << "\nПример данных (" << min(count, packages.size()) << " записей):" << endl;
        for (size_t i = 0; i < min(count, packages.size()); i++) {
            cout << "[" << i + 1 << "] Код: " << packages[i].code 
                 << ", Город: " << packages[i].city 
                 << ", Получатель: " << packages[i].recipient << endl;
        }
    }
    
    void print_statistics() const {
        cout << "\nСтатистика данных:" << endl;
        cout << "Всего записей: " << packages.size() << endl;
        cout << "Примерный размер в памяти: " 
             << (packages.size() * sizeof(Package) / 1024 / 1024) << " MB" << endl;
        
        // Подсчет уникальных городов
        unordered_map<string, int> city_counts;
        for (const auto& pkg : packages) {
            city_counts[pkg.city]++;
        }
        
        cout << "Уникальных городов: " << city_counts.size() << endl;
        for (const auto& city_pair : city_counts) {
            cout << "  " << city_pair.first << ": " << city_pair.second << " посылок" << endl;
        }
    }
    
    size_t get_total_count() const {
        return packages.size();
    }
    
    // Метод для тестирования разных размеров данных
    void benchmark_different_sizes() {
        vector<size_t> test_sizes = {100, 1000, 10000, 100000, 1000000};
        string pattern = "PKG-.*";
        
        cout << "\nБЕНЧМАРК РАЗНЫХ РАЗМЕРОВ ДАННЫХ" << endl;
        cout << "=================================" << endl;
        
        for (size_t size : test_sizes) {
            cout << "\n\nРазмер данных: " << size << endl;
            cout << string(40, '-') << endl;
            
            // Создаем временный процессор для тестирования
            vector<Package> temp_packages;
            temp_packages.reserve(size);
            
            // Генерируем данные
            vector<string> cities = {"Москва", "Санкт1", "Санкт2"};
            vector<string> surnames = {"Иванов", "Петров"};
            vector<string> names = {"Алексей", "Дмитрий"};
            
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<int> dist(0, 1000);
            
            for (size_t i = 0; i < size; i++) {
                string code = "PKG-" + to_string(1000 + dist(gen));
                string city = cities[i % cities.size()];
                string recipient = surnames[i % surnames.size()] + " " + names[i % names.size()];
                temp_packages.emplace_back(code, city, recipient);
            }
            
            // Однопоточный тест
            auto start = chrono::high_resolution_clock::now();
            vector<string> single_results;
            regex pattern_regex(pattern);
            
            for (const auto& pkg : temp_packages) {
                if (regex_match(pkg.code, pattern_regex)) {
                    single_results.push_back(pkg.recipient);
                }
            }
            
            auto end = chrono::high_resolution_clock::now();
            auto single_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();
            
            // Многопоточный тест (фиксировано 4 потока)
            start = chrono::high_resolution_clock::now();
            vector<string> multi_results;
            int num_threads = min(4, static_cast<int>(size / 100 + 1));
            
            vector<thread> threads;
            mutex mtx;
            size_t chunk_size = size / static_cast<size_t>(num_threads);
            
            for (int t = 0; t < num_threads; t++) {
                size_t start_idx = static_cast<size_t>(t) * chunk_size;
                size_t end_idx = (t == num_threads - 1) ? size : start_idx + chunk_size;
                
                threads.emplace_back([&, start_idx, end_idx]() {
                    vector<string> local_results;
                    for (size_t i = start_idx; i < end_idx; i++) {
                        if (regex_match(temp_packages[i].code, pattern_regex)) {
                            local_results.push_back(temp_packages[i].recipient);
                        }
                    }
                    if (!local_results.empty()) {
                        lock_guard<mutex> lock(mtx);
                        multi_results.insert(multi_results.end(), 
                                            local_results.begin(), local_results.end());
                    }
                });
            }
            
            for (auto& t : threads) t.join();
            end = chrono::high_resolution_clock::now();
            auto multi_time = chrono::duration_cast<chrono::milliseconds>(end - start).count();
            
            cout << "Найдено получателей: " << single_results.size() << endl;
            cout << "Однопоточное время: " << single_time << " мс" << endl;
            cout << "Многопоточное время: " << multi_time << " мс" << endl;
            cout << "Ускорение: " << (single_time > 0 ? static_cast<double>(single_time) / multi_time : 0) << "x" << endl;
            cout << "Потоков использовано: " << num_threads << endl;
        }
    }
};

int main() {
    cout << "ТЕСТ 1: БОЛЬШИЕ ДАННЫЕ (1,000,000 записей)" << endl;
    
    PackageProcessor processor_big(1000000);
    processor_big.print_statistics();
    processor_big.print_sample(3);
    
    string pattern = "PKG-.*";
    
    auto [single_results_big, single_time_big] = processor_big.process_single(pattern);
    auto [multi_results_big, multi_time_big] = processor_big.process_multi(pattern, 8);
    
    cout << "\nРезультаты (большие данные):" << endl;
    cout << "Однопоточное время: " << single_time_big << " мс" << endl;
    cout << "Многопоточное время: " << multi_time_big << " мс" << endl;
    cout << "Ускорение: " << (single_time_big > 0 ? static_cast<double>(single_time_big) / multi_time_big : 0) << "x" << endl;
    
    cout << "\n\nТЕСТ 2: МАЛЫЕ ДАННЫЕ (100 записей)" << endl;
    
    PackageProcessor processor_small(100);
    processor_small.print_statistics();
    processor_small.print_sample(5);
    
    auto [single_results_small, single_time_small] = processor_small.process_single(pattern);
    auto [multi_results_small, multi_time_small] = processor_small.process_multi(pattern, 4);
    
    cout << "\nРезультаты (малые данные):" << endl;
    cout << "Однопоточное время: " << single_time_small << " мс" << endl;
    cout << "Многопоточное время: " << multi_time_small << " мс" << endl;
    cout << "Ускорение: " << (single_time_small > 0 ? static_cast<double>(single_time_small) / multi_time_small : 0) << "x" << endl;

    cout << "\n\nКОМПЛЕКСНЫЙ БЕНЧМАРК" << endl;
    PackageProcessor bench_processor(1000);
    bench_processor.benchmark_different_sizes();
    
    return 0;
}