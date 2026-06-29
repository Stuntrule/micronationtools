#pragma once

#include <vector>
#include <cmath>
#include <iostream>

using namespace std;

class sort_hc
{
    private: 

    vector<int> temp = {};
    vector<int> temp2 = {};
    vector<bool> temp3 = {};
    int a;
    int b;
    int c;
    int d;
    int e;
    int len;
    bool sorted;
    bool sort2;

    vector<int> countv = {};
    vector<int> ans = {};
    int minv;
    int maxv;
    int len;

    void vprinta(vector<int> &vec) {
        for (int i=0; i < vec.size(); i++) {
            cout << vec[i] << " ";
        }
    }
    
    public:
    vector<int> bubblesort(vector<int> &vec) {
        temp = vec;
        len = vec.size();
        sorted = false;
        sort2 = false;
        a = 0;
        while (!sorted) {
            sort2 = false;
            for (int i = 1; i < len-1; i++) {
                if ((i == len-1) && !sort2) sorted = true;
                if (temp[i-1] > temp[i]) {
                    a = temp[i];
                    temp[i] = temp[i-1];
                    temp[i-1] = a;
                    sort2 = true;
                }
            }
            if (temp[len-2] > temp[len-1]) {
                a = temp[len-1];
                temp[len-1] = temp[len-2];
                temp[len-2] = a;
                sort2 = true;
            }
            if (!sort2) sorted = true;
        }
        return temp;
    }

    vector<int> argbasort(vector<int> &vec) {
        temp = {};
        temp2 = vec;
        a = vec[0];
        b = 0;
        len = vec.size();
        for (int i = 0; i < len; i++) {
            a = temp2[0];
            b = 0;
            for (int j = i; j < len; j++) {
                if (temp2[j-i] < a) {
                    b = j-i;
                    a = temp2[b];
                }
            }
            temp2.erase(temp2.begin() + b);
            temp.push_back(a);
        }
        return temp;
    }

    vector<int> selectionsort(vector<int> &vec) {
        temp2 = vec;
        a = vec[0];
        b = 0;
        c = 0;
        len = vec.size();
        for (int i = 0; i < len; i++) {
            a = temp2[i];
            b = i;
            for (int j = i; j < len; j++) {
                if (temp2[j] < a) {
                    b = j;
                    a = temp2[b];
                }
            }
            c = temp2[i];
            temp2[i] = a;
            temp2[b] = c;

        }
        return temp2;
    }

    vector<int> argbaminmaxsort(vector<int> &vec) {
        len = vec.size();
        temp = vec;
        temp2 = vec;
        e = 0;
        while (temp.size() != 0) {
            a = temp[0];
            c = a;
            b = 0;
            d = 0;
            for (int i = 0; i < temp.size(); i++) {
                if (temp[i] > c) {
                    c = temp[i];
                    d = i;
                } else if (temp[i] < a) {
                    a = temp[i];
                    b = i;
                }
            }
            temp2[e] = a;
            temp.erase(temp.begin() + b);
            if (b != d) {
                if (d > b) d--;
                temp.erase(temp.begin() + d);
                temp2[len-e-1] = c;
            }
            e++;
        }
        return temp2;
    }

    vector<int> positionsort(vector<int> &vec) {
        int n = vec.size();
        if (n <= 1) return vec;

        temp = vec;

        // Step 1: Find min and max
        int min_val = temp[0];
        int max_val = temp[0];
        for (int i = 1; i < n; i++) {
            if (temp[i] < min_val) min_val = temp[i];
            if (temp[i] > max_val) max_val = temp[i];
        }

        if (min_val == max_val) return temp;

        vector<int> result(n, -1);   // -1 means empty slot
        int range = max_val - min_val;

        // Step 2: Mathematical positioning with collision probing
        for (int i = 0; i < n; i++) {
            int x = temp[i];

            // Calculate target position
            long long num = static_cast<long long>(x - min_val) * (n - 1);
            int d = (num + (range / 2)) / range;  // rounded

            if (d < 0) d = 0;
            if (d >= n) d = n - 1;

            // Try exact position
            if (result[d] == -1) {
                result[d] = x;
                continue;
            }

            // Collision: Probe outward (Right priority, then Left)
            bool placed = false;
            int offset = 1;
            while (!placed && offset < n) {
                if (d + offset < n && result[d + offset] == -1) {
                    result[d + offset] = x;
                    placed = true;
                }
                else if (d - offset >= 0 && result[d - offset] == -1) {
                    result[d - offset] = x;
                    placed = true;
                }
                offset++;
            }

            // Fallback
            if (!placed) {
                for (int j = n - 1; j >= 0; j--) {
                    if (result[j] == -1) {
                        result[j] = x;
                        break;
                    }
                }
            }
        }

        // Step 3: Final INSERTION SORT (better for nearly-sorted data)
        for (int i = 1; i < n; i++) {
            int key = result[i];
            int j = i - 1;

            while (j >= 0 && result[j] > key) {
                result[j + 1] = result[j];
                j--;
            }
            result[j + 1] = key;
        }

        return result;
    }

    vector<int> insertionsort(vector<int> a) {
        int n = a.size();

        for (int i = 1; i < n; i++) {
            int key = a[i];
            int j = i - 1;

            while (j >= 0 && a[j] > key) {
                a[j + 1] = a[j];
                j--;
            }

            a[j + 1] = key;
        }

        return a;
    }

    vector<int> quicksort(vector<int> a) {
        if (a.size() <= 1) return a;

        int pivot = a[a.size() / 2];

        vector<int> left;
        vector<int> mid;
        vector<int> right;

        left.reserve(a.size());
        right.reserve(a.size());
        mid.reserve(1);

        for (int x : a) {
            if (x < pivot) left.push_back(x);
            else if (x > pivot) right.push_back(x);
            else mid.push_back(x);
        }

        vector<int> sortedLeft = quicksort(left);
        vector<int> sortedRight = quicksort(right);

        vector<int> result;
        result.reserve(a.size());

        result.insert(result.end(), sortedLeft.begin(), sortedLeft.end());
        result.insert(result.end(), mid.begin(), mid.end());
        result.insert(result.end(), sortedRight.begin(), sortedRight.end());

        return result;
    }

    vector<int> countingsort(vector<int> &vec) {
        minv = vec[0];
        maxv = minv;
        len = vec.size();
        ans = {};
        for (int i = 0; i < len; i++) {
            if (vec[i] < minv) minv = vec[i];
            else if (vec[i] > maxv) maxv = vec[i];
        }
        int range = maxv-minv+1;
        countv.assign(range, 0);
        for (int i = 0; i < len; i++) {
            countv[vec[i]-minv]++;
        }
        for (int i = 0; i < range; i++) {
            for (int j = 0; j < countv[i]; j++) {
                ans.push_back(i+minv);
            }
        }
        return ans;
    }
};
