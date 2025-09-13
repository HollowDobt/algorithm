#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>

using namespace std;

static int precedence(char op) {
    switch (op) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
        default:
            return -1;  // '(' or invalid
    }
}

static bool isLeftAssociative(char op) {
    // 这里四则运算均是左结合
    return (op == '+' || op == '-' || op == '*' || op == '/');
}

static bool isOperator(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/');
}

static double applyOp(double a, double b, char op) {
    switch (op) {
        case '+':
            return a + b;
        case '-':
            return a - b;
        case '*':
            return a * b;
        case '/':
            if (b == 0.0) throw runtime_error("Division by zero");
            return a / b;
        default:
            throw runtime_error(string("Unknown operator: ") + op);
    }
}

// 在需要时，按照优先级/结合性进行规约
static void reduceTop(stack<double>& values, stack<char>& ops) {
    if (ops.empty())
        throw runtime_error("Operator stack empty during reduction");
    char op = ops.top();
    ops.pop();

    if (values.size() < 2) throw runtime_error("Insufficient operands");
    double rhs = values.top();
    values.pop();
    double lhs = values.top();
    values.pop();

    values.push(applyOp(lhs, rhs, op));
}

int main(int argc, char const* argv[]) {
    if (argc != 2) {
        cerr << "Illegal operation: The operand should be 2, such as: ./a.out "
                "\"1+1\""
             << endl;
        return 1;
    }

    string s = argv[1];
    stack<double> values;  // 数值栈
    stack<char> ops;       // 运算符/括号栈

    enum Prev { START, NUMBER, RIGHT_PAREN, OPERATOR };
    Prev prev = START;

    // 逐字符扫描
    for (size_t i = 0; i < s.size();) {
        char c = s[i];

        // 跳过空白
        if (isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        if (isdigit(static_cast<unsigned char>(c)) || c == '.') {
            // 读取数（支持小数）
            size_t j = i;
            bool dotSeen = (c == '.');
            ++j;
            while (j < s.size()) {
                char d = s[j];
                if (isdigit(static_cast<unsigned char>(d))) {
                    ++j;
                } else if (d == '.' && !dotSeen) {
                    dotSeen = true;
                    ++j;
                } else
                    break;
            }
            double val = 0.0;
            try {
                val = stod(s.substr(i, j - i));
            } catch (...) {
                cerr << "Invalid number near: \"" << s.substr(i, j - i) << "\""
                     << endl;
                return 1;
            }
            values.push(val);
            prev = NUMBER;
            i = j;
            continue;
        }

        if (c == '(') {
            // “(” 直接入栈
            ops.push('(');
            prev = OPERATOR;  // 括号后可视作期待右操作数，一元符号可成立
            ++i;
            continue;
        }

        if (c == ')') {
            // 弹到匹配的“(”
            bool matched = false;
            while (!ops.empty()) {
                char top = ops.top();
                if (top == '(') {
                    matched = true;
                    ops.pop();
                    break;
                }
                reduceTop(values, ops);
            }
            if (!matched) {
                cerr << "Unmatched right parenthesis ')'" << endl;
                return 1;
            }
            prev = RIGHT_PAREN;
            ++i;
            continue;
        }

        if (isOperator(c)) {
            // 处理一元 ±：当出现在表达式开头、左括号后、其他运算符后
            bool
                unary =
                    (prev == START || prev == OPERATOR || prev == RIGHT_PAREN /* 右括号后的一元可按二元处理：-(...) 需要在 '(' 前判断，这里 RIGHT_PAREN 不作为一元触发条件 */) &&
                    (c == '+' || c == '-');

            if (unary) {
                // 将一元 ±x 规约为 0 ± x 的二元形式：
                // 情形1：后面直接是数字 => 把数字读入并压入，再按普通二元运算与
                // 0 结合 情形2：后面是 '(' => 先压入 0 与此一元符号（作为二元
                // -），随后 '(' 正常处理 统一做法：立即压入一个
                // 0，再把当前符号当作二元运算符处理
                values.push(0.0);
            }

            // 在压入当前运算符前，按优先级/结合性规约
            while (!ops.empty()) {
                char top = ops.top();
                if (top == '(') break;
                int pt = precedence(top), pc = precedence(c);
                if (pt > pc || (pt == pc && isLeftAssociative(c))) {
                    reduceTop(values, ops);
                } else
                    break;
            }
            ops.push(c);
            prev = OPERATOR;
            ++i;
            continue;
        }

        // 其他非法字符
        cerr << "Invalid character: '" << c << "'" << endl;
        return 1;
    }

    // 收尾：弹空运算符栈
    while (!ops.empty()) {
        if (ops.top() == '(') {
            cerr << "Unmatched left parenthesis '('" << endl;
            return 1;
        }
        reduceTop(values, ops);
    }

    if (values.size() != 1) {
        cerr << "Invalid expression: remaining " << values.size() << " values"
             << endl;
        return 1;
    }

    // 结果输出：尽量以合理格式打印（整数则不显示小数）
    double result = values.top();
    if (fabs(result - llround(result)) < 1e-12) {
        cout << llround(result) << endl;
    } else {
        // 控制小数位，避免 0.30000000004 之类
        ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(12);
        oss << result;
        string out = oss.str();
        // 去除多余尾随 0
        auto pos = out.find('.');
        if (pos != string::npos) {
            size_t end = out.size();
            while (end > pos + 1 && out[end - 1] == '0') --end;
            if (end > pos + 1 && out[end - 1] == '.') --end;
            out.resize(end);
        }
        cout << out << endl;
    }
    return 0;
}
