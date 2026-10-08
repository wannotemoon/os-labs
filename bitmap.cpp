#include "bitmap.h"

#include <iostream>
#include <stdexcept>
using namespace std;

Bitmap::Bitmap(int blockCount) : blockCount(blockCount) {
    if (blockCount <= 0) {
        throw invalid_argument("Block count must be positive");
    }
    int byteCount = blockCount / 8 + (blockCount % 8 != 0);
    data.assign(byteCount, 0);
}

bool Bitmap::isUsed(int no) const {
    if (no < 0 || no >= blockCount) {
        throw out_of_range("Block number is invalid");
    }
    int byteNo = no / 8;
    int bitNo = no % 8;
    unsigned int mask = 1u << bitNo;
    return (data[byteNo] & mask) != 0;
}

void Bitmap::setUsed(int no, bool used) {
    if (no < 0 || no >= blockCount) {
        throw out_of_range("Block number is invalid");
    }
    int byteNo = no / 8;
    int bitNo = no % 8;
    unsigned int mask = 1u << bitNo;
    if (used) {
        data[byteNo] = static_cast<unsigned char>(data[byteNo] | mask);
    } else {
        data[byteNo] = static_cast<unsigned char>(data[byteNo] & ~mask);
    }
}

int Bitmap::getFreeCount() const {
    int count = 0;
    for (int no = 0; no < blockCount; no++) {
        if (!isUsed(no)) {
            count++;
        }
    }
    return count;
}

bool Bitmap::findFreeBlocks(int count, vector<int>& blocks) const {
    blocks.clear();
    if (count <= 0 || count > blockCount) {
        return false;
    }
    for (int no = 0; no < blockCount; no++) {
        if (!isUsed(no)) {
            blocks.push_back(no);
            if (static_cast<int>(blocks.size()) == count) {
                return true;
            }
        }
    }
    blocks.clear();
    return false;
}

void Bitmap::show() const {
    cout << "Blocks: " << blockCount << endl;
    for (int no = 0; no < blockCount; no++) {
        cout << no << ":" << (isUsed(no) ? 1 : 0) << " ";
        if (no % 8 == 7 || no == blockCount - 1) {
            cout << endl;
        }
    }
}