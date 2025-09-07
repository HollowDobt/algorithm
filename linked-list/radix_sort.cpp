#include <cstdint>
#include <list>
#include <vector>

using namespace std;
using u64 = uint64_t;

vector<u64> radixSort(vector<u64> const& lst_c) {
    vector<u64> lst(lst_c);
    u64 pos = 0, exp = 1;
    while(++pos <= 10 && (exp *= 10)) {
        vector<list<u64>> buc(10);
        for(auto const& num : lst) {
            buc[(num/(exp/10)) % 10].push_back(num);
        }
        lst.clear();
        for(auto const& ls : buc) {
            for (auto const& num : ls) {
                lst.push_back(num);
            }
        }
    }
    return lst;
}