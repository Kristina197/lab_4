#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

using namespace std;

class BankersAlgorithm {
private:
    vector<int> available;
    vector<vector<int>> max;
    vector<vector<int>> allocation;
    vector<vector<int>> need;
    int n;
    int m;
    
    void print_vector(const string& name, const vector<int>& vec) {
        cout << name << ": [";
        for (size_t i = 0; i < vec.size(); i++) {
            cout << vec[i];
            if (i < vec.size() - 1) cout << ", ";
        }
        cout << "]" << endl;
    }
    
    void print_matrix(const string& name, const vector<vector<int>>& matrix) {
        cout << name << ":\n";
        for (int i = 0; i < n; i++) {
            cout << "  P" << i << ": [";
            for (int j = 0; j < m; j++) {
                cout << matrix[i][j];
                if (j < m - 1) cout << ", ";
            }
            cout << "]" << endl;
        }
    }
    
    bool is_safe_state(vector<int>& safe_sequence) {
        vector<int> work = available;
        vector<bool> finish(n, false);
        safe_sequence.clear();
        
        cout << "\nПроверка безопасного состояния:\n";
        
        for (int count = 0; count < n; count++) {
            bool found = false;
            
            for (int p = 0; p < n; p++) {
                if (!finish[p]) {
                    bool can_allocate = true;
                    
                    for (int j = 0; j < m; j++) {
                        if (need[p][j] > work[j]) {
                            can_allocate = false;
                            break;
                        }
                    }
                    
                    if (can_allocate) {
                        for (int j = 0; j < m; j++) {
                            work[j] += allocation[p][j];
                        }
                        
                        finish[p] = true;
                        safe_sequence.push_back(p);
                        found = true;
                        
                        cout << "  P" << p << " добавлен в безопасную последовательность\n";
                        break;
                    }
                }
            }
            
            if (!found) {
                cout << "  Небезопасное состояние!\n";
                return false;
            }
        }
        
        return true;
    }

public:
    BankersAlgorithm() {
        n = 5;
        m = 3;
        
        available = {3, 3, 2};
        
        max = {
            {7, 5, 3},
            {3, 2, 2},
            {9, 0, 2},
            {2, 2, 2},
            {4, 3, 3}
        };
        
        allocation = {
            {0, 1, 0},
            {2, 0, 0},
            {3, 0, 2},
            {2, 1, 1},
            {0, 0, 2}
        };
        
        need.resize(n, vector<int>(m));
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) {
                need[i][j] = max[i][j] - allocation[i][j];
            }
        }
    }
    
    void run() {
        cout << "Алгоритм банкира\n";
        
        cout << "Инициализация системы:\n";
        print_vector("Доступные ресурсы", available);
        print_matrix("Максимальные потребности", max);
        print_matrix("Выделенные ресурсы", allocation);
        print_matrix("Потребности", need);
        
        vector<int> safe_sequence;
        if (is_safe_state(safe_sequence)) {
            cout << "\nСистема находится в безопасном состоянии!\n";
            cout << "Безопасная последовательность: ";
            for (size_t i = 0; i < safe_sequence.size(); i++) {
                cout << "P" << safe_sequence[i];
                if (i < safe_sequence.size() - 1) cout << " → ";
            }
            cout << endl;
        } else {
            cout << "\nСистема находится в небезопасном состоянии!\n";
        }
        
        test_resource_requests();
    }
    
    void test_resource_requests() {
        cout << "\n\nТестирование запросов ресурсов:\n";
        
        cout << "\n1. Безопасный запрос:\n";
        int process_id = 1;
        vector<int> request = {1, 0, 2};
        
        cout << "   P" << process_id << " запрашивает: ";
        print_vector("Request", request);
        
        if (request_resources(process_id, request)) {
            cout << "   Запрос удовлетворен\n";
        } else {
            cout << "   Запрос отклонен\n";
        }
        
        cout << "\n2. Небезопасный запрос:\n";
        process_id = 0;
        request = {0, 2, 0};
        
        cout << "   P" << process_id << " запрашивает: ";
        print_vector("Request", request);
        
        if (request_resources(process_id, request)) {
            cout << "   Запрос удовлетворен\n";
        } else {
            cout << "   Запрос отклонен\n";
        }
    }
    
    bool request_resources(int process_id, vector<int> request) {
        cout << "\n   Обработка запроса от P" << process_id << ":\n";
        
        for (int j = 0; j < m; j++) {
            if (request[j] > need[process_id][j]) {
                cout << "     Ошибка: превышена потребность\n";
                return false;
            }
        }
        
        for (int j = 0; j < m; j++) {
            if (request[j] > available[j]) {
                cout << "     Ошибка: недостаточно ресурсов\n";
                return false;
            }
        }
        
        vector<int> old_available = available;
        vector<vector<int>> old_allocation = allocation;
        vector<vector<int>> old_need = need;
        
        for (int j = 0; j < m; j++) {
            available[j] -= request[j];
            allocation[process_id][j] += request[j];
            need[process_id][j] -= request[j];
        }
        
        vector<int> safe_sequence;
        if (is_safe_state(safe_sequence)) {
            return true;
        } else {
            available = old_available;
            allocation = old_allocation;
            need = old_need;
            return false;
        }
    }
};

int main() {    
    BankersAlgorithm bankers;
    bankers.run();
    
    return 0;
}