#include <cstddef>
#include <fstream>
#include <iostream>
#include <stack>
#include <string>

using namespace std;

int main(int argc, char const* argv[]) {
    // 其中另一个参数 argv[1] 指向需要检查语法的文件地址
    if (argc != 2) {
        cerr << "Input type is undefined" << endl;
        return 1;
    }

    ifstream inFile(argv[1]);
    string leftLst = "([{", righLst = ")]}";

    if (!inFile) {
        cerr << "No such file: " << argv[1] << endl;
        return 1;
    }

    string line;
    stack<pair<size_t, pair<size_t, size_t>>> lineRead;
    size_t lineNum = 0;

    while (getline(inFile, line) && ++lineNum) {
        size_t rowNum = 0;

        for (char const& ch : line) {
            ++rowNum;

            bool handled = false;

            for (size_t ptr_ex = 0; ptr_ex < leftLst.size(); ++ptr_ex) {
                if (ch == leftLst[ptr_ex]) {
                    lineRead.push({ptr_ex, {lineNum, rowNum}});
                    handled = true;
                    break;
                }
            }

            if (handled == true) continue;

            for (size_t ptr_ex = 0; ptr_ex < righLst.size(); ++ptr_ex) {
                if (ch == righLst[ptr_ex]) {
                    if (lineRead.empty() == true ||
                        lineRead.top().first != ptr_ex) {
                        cout << "Unpaired symbol detected\n"
                             << "In [line, row] -> [" << lineNum << ", "
                             << rowNum << "]: unpaired symbol `" << ch << "`"
                             << endl;

                        if (lineRead.empty() == false) {
                            cout << "Expected symbol: "
                                 << righLst[lineRead.top().first] << endl;
                        } else {
                            cout << "No left symbol found." << endl;
                        }
                        return 0;
                    }

                    lineRead.pop();
                    break;
                }
            }
        }
    }

    if (lineRead.empty() != true) {
        cout << "Unpaired symbol detected\n"
             << "In [line, row] -> [" << lineRead.top().second.first << ", "
             << lineRead.top().second.second << "]: unpaired symbol `"
             << leftLst[lineRead.top().first] << "`" << endl;

        return 0;
    }

    cout << "No error found." << endl;

    return 0;
}
