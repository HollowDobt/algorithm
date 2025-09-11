#include <cstddef>
#include <iostream>
#include <stdexcept>

using namespace std;

template <typename T, size_t N>
class Stack {
   private:
    size_t top_ptr;
    T array[N];

   public:
    Stack() : top_ptr(0) {}
    // 使用的是栈空间, 不需要额外的析构函数

    T& top() {
        if (top_ptr > 0) {
            return array[top_ptr - 1];
        }
        throw runtime_error("Stack is empty!");
    }

    T const& top() const {
        if (top_ptr > 0) {
            return array[top_ptr - 1];
        }
        throw runtime_error("Stack is empty!");
    }

    // 习惯上不做边界检查, 避免过多运行时间
    void push(T const& num) { array[++top_ptr - 1] = num; }
    T pop() { return array[top_ptr-- - 1]; }
    size_t const& size() const noexcept { return top_ptr; }
};

int main(void) {
    Stack<int, 1000> stack;
    stack.push(100);
    cout << stack.pop() << " " << stack.size() << endl;

    return 0;
}
