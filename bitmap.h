//
// Created by 28794 on 2026/10/8.
//

#ifndef OS_BITMAP_H
#define OS_BITMAP_H

#include <vector>
using namespace std;

class Bitmap {
private:
    int blockCount;
    vector<unsigned char> data;

public:
    Bitmap(int blockCount);

    bool isUsed(int no) const;

    void setUsed(int no,bool used);

    bool findFreeBlocks(int count,vector<int>& blocks) const;

    int getFreeCount() const;

    void show() const;
};

#endif //OS_BITMAP_H
