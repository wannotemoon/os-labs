#include "file_system.h"
#include <cstdint>
#include <limits>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <utility>

/* [第九轮：准备FileSystem对象，以及打开虚盘]
 * 构造函数只准备成员；open成功之后，currentPath才含有根目录入口。
 * 根目录的内容已经保存在0号块，不需要在根目录里再写一条“根自己的FCB”。
 */
FileSystem::FileSystem(int blockCount) : disk(blockCount) {
    // currentPath和lastError默认都是空的；此时尚未打开任何文件。
}

bool FileSystem::open(const string &diskPath) {
    if (!disk.openDisk(diskPath)) {
        lastError=disk.getLastError();
        return false;
    }
    currentPath.clear();
    currentPath.push_back({"",DiskManager::ROOT_BLOCK});
    lastError="";
    return true;
}

// [职责边界] open沿用openDisk的文件长度和FAT表项范围校验，不等于整盘完整性检查。
// 后续目录操作仍须用getChain/readEntry检查实际访问的链和目录项，不能直接信任磁盘内容。
// 同一对象重复open由openDisk拒绝；不会偷偷关闭原磁盘或切换到另一文件。

/* [第八轮：固定32字节目录项读写——本次只实现这两个内部方法]
 * 一块1024字节可以放32个目录项，entryIndex是槽位号，不是文件字节偏移。
 * 块内起点 = entryIndex * 32；块在虚盘中的位置仍由DiskManager负责换算。
 *
 * 每条记录的布局：
 *   0..7   名称8字节，不足补0，恰好8字节时不再附加结束符
 *   8..11  大小，4字节小端
 *   12..15 首块号，4字节小端；-1编码为FF FF FF FF
 *   16     类型：数值0/1/2，不是字符'0'/'1'/'2'
 *   17..31 日期15字节：yyyymmdd hhmmss，不附加结束符
 *
 * [职责边界]
 * 这里只检查记录的字段格式，不判断名称重名、路径语法、实际FAT链归属和链长度。
 * UTF-8名称按字节保存，不在这里解码；文件名保留字等业务限制留给isValidName。
 * 日期只检查15字节的数字/空格形状，实际日期由以后makeDateTime生成。
 * 整数先在uint32_t中拼接并检查范围，不能把高位非法值直接当成int或数组下标。
 * 不直接写FCB对象：string对象内部含有管理信息，不是名称和日期的磁盘字节。
 */

bool FileSystem::readEntry(int blockNo, int entryIndex, FCB &entry) {
    if (entryIndex<0 || entryIndex>=DiskManager::BLOCK_SIZE/FCB::DISK_SIZE) {
        lastError="entry index out of range";
        return false;
    }
    vector<char> buffer;
    if (!disk.readBlock(blockNo,buffer)) {
        lastError=disk.getLastError();
        return false;
    }
    size_t offset=static_cast<size_t>(entryIndex)*FCB::DISK_SIZE;
    unsigned char type=static_cast<unsigned char>(buffer[offset+16]);
    if (type==0) {
        entry=FCB{};
        lastError="";
        return true;
    }
    if (type!=1 && type!=2) {
        lastError="invalid FCB type";
        return false;
    }

    FCB decoded;
    decoded.type=static_cast<char>(type);
    for (int index=0;index<FCB::NAME_SIZE && buffer[offset+index]!='\0';index++) {
        decoded.name.push_back(buffer[offset+index]);
    }
    if (decoded.name.empty()) {
        lastError="FCB name must not be empty";
        return false;
    }
    uint32_t sizeValue=0;
    uint32_t firstValue=0;
    for (int index=0;index<4;index++) {
        // 先转unsigned char，避免80、FF等高位字节被符号扩展。
        sizeValue|=static_cast<uint32_t>(static_cast<unsigned char>(buffer[offset+8+index]))<<(index*8);
        firstValue|=static_cast<uint32_t>(static_cast<unsigned char>(buffer[offset+12+index]))<<(index*8);
    }
    if (sizeValue>static_cast<uint32_t>(numeric_limits<int>::max()) ||
        (firstValue!=UINT32_MAX && firstValue>static_cast<uint32_t>(numeric_limits<int>::max()))) {
        lastError="FCB integer out of range";
        return false;
    }
    decoded.size=static_cast<int>(sizeValue);
    decoded.firstBlock=firstValue==UINT32_MAX ? -1 : static_cast<int>(firstValue);
    if (decoded.type==1) {
        if ((decoded.size==0 && decoded.firstBlock!=-1) || (decoded.size>0 && decoded.firstBlock<1)) {
            lastError="invalid file size or first block";
            return false;
        }
    } else if (decoded.size<DiskManager::BLOCK_SIZE || decoded.size%DiskManager::BLOCK_SIZE!=0 ||
               decoded.firstBlock<1) {
        lastError="invalid directory size or first block";
        return false;
    }
    decoded.datetime.assign(buffer.data()+offset+17,FCB::DATETIME_SIZE);
    for (int index=0;index<FCB::DATETIME_SIZE;index++) {
        if ((index==8 && decoded.datetime[index]!=' ') ||
            (index!=8 && (decoded.datetime[index]<'0' || decoded.datetime[index]>'9'))) {
            lastError="invalid FCB datetime";
            return false;
        }
    }
    entry=decoded;
    lastError="";
    return true;
}

bool FileSystem::writeEntry(int blockNo, int entryIndex, const FCB &entry) {
    if (entryIndex<0 || entryIndex>=DiskManager::BLOCK_SIZE/FCB::DISK_SIZE) {
        lastError="entry index out of range";
        return false;
    }
    if (entry.type!=0 && entry.type!=1 && entry.type!=2) {
        lastError="invalid FCB type";
        return false;
    }
    // [讲解] 类型0只表示使槽位失效，不需要旧名称、日期、首块仍然合法。
    if (entry.type!=0) {
        if (entry.name.empty() || entry.name.size()>static_cast<size_t>(FCB::NAME_SIZE) ||
            entry.name.find('\0')!=string::npos) {
            lastError="FCB name must be 1..8 bytes without NUL";
            return false;
        }
        if (entry.size<0) {
            lastError="FCB size must not be negative";
            return false;
        }
        if (entry.type==1) {
            if ((entry.size==0 && entry.firstBlock!=-1) || (entry.size>0 && entry.firstBlock<1)) {
                lastError="invalid file size or first block";
                return false;
            }
        } else if (entry.size<DiskManager::BLOCK_SIZE || entry.size%DiskManager::BLOCK_SIZE!=0 ||
                   entry.firstBlock<1) {
            lastError="invalid directory size or first block";
            return false;
        }
        if (entry.datetime.size()!=static_cast<size_t>(FCB::DATETIME_SIZE)) {
            lastError="FCB datetime must be 15 bytes";
            return false;
        }
        for (int index=0;index<FCB::DATETIME_SIZE;index++) {
            if ((index==8 && entry.datetime[index]!=' ') ||
                (index!=8 && (entry.datetime[index]<'0' || entry.datetime[index]>'9'))) {
                lastError="invalid FCB datetime";
                return false;
            }
        }
    }

    // [讲解] 磁盘按整块读写，所以先读1024字节，只改其中一个32字节槽，再写回整块。
    vector<char> buffer;
    if (!disk.readBlock(blockNo,buffer)) {
        lastError=disk.getLastError();
        return false;
    }
    size_t offset=static_cast<size_t>(entryIndex)*FCB::DISK_SIZE;
    if (entry.type==0) {
        buffer[offset+16]=0; // 沿用讲义：删除只改类型，info中仍可能看见旧名称。
    } else {
        for (int index=0;index<FCB::DISK_SIZE;index++) {
            buffer[offset+index]=0;
        }
        for (size_t index=0;index<entry.name.size();index++) {
            buffer[offset+index]=entry.name[index];
        }
        uint32_t sizeValue=static_cast<uint32_t>(entry.size);
        // 非负块号按原值写；-1转uint32_t后为0xFFFFFFFF，明确约定这4个FF表示空链。
        uint32_t firstValue=static_cast<uint32_t>(entry.firstBlock);
        for (int index=0;index<4;index++) {
            buffer[offset+8+index]=static_cast<char>((sizeValue>>(index*8))&0xFFu);
            buffer[offset+12+index]=static_cast<char>((firstValue>>(index*8))&0xFFu);
        }
        buffer[offset+16]=entry.type;
        for (int index=0;index<FCB::DATETIME_SIZE;index++) {
            buffer[offset+17+index]=entry.datetime[index];
        }
    }
    if (!disk.writeBlock(blockNo,buffer)) {
        lastError=disk.getLastError();
        return false;
    }
    lastError="";
    return true;
}

/* [剩余功能一：目录读取与校验]
 * EntryPosition是本次操作的临时结果，包含FCB和它所在的块/槽位。
 * 名称查找、重用删除槽、更新父目录中的大小，都需要这个位置。
 */
bool FileSystem::isValidName(const string &name) {
    if (name.empty() || name.size()>static_cast<size_t>(FCB::NAME_SIZE) || name=="." || name=="..") {
        lastError="name must be 1..8 UTF-8 bytes, excluding . and ..";
        return false;
    }
    for (unsigned char ch : name) {
        if (ch<=32 || ch==127 || string("<>:\"/\\|?*").find(static_cast<char>(ch))!=string::npos) {
            lastError="name contains whitespace, control characters or reserved punctuation";
            return false;
        }
    }
    lastError="";
    return true;
}

string FileSystem::makeDateTime() const {
    time_t now=time(nullptr);
    if (now==static_cast<time_t>(-1)) {
        return "";
    }
    const tm* local=localtime(&now);
    if (local==nullptr) {
        return "";
    }
    char buffer[FCB::DATETIME_SIZE+1]{};
    if (strftime(buffer,sizeof(buffer),"%Y%m%d %H%M%S",local)!=FCB::DATETIME_SIZE) {
        return "";
    }
    return buffer;
}

bool FileSystem::readDirectory(int firstBlock,vector<EntryPosition> &entries) {
    entries.clear();
    vector<int> blocks;
    if (!disk.getChain(firstBlock,blocks)) {
        lastError=disk.getLastError();
        return false;
    }
    if (blocks.empty()) {
        lastError="directory has no data blocks";
        return false;
    }
    vector<EntryPosition> result;
    for (int block : blocks) {
        for (int index=0;index<DiskManager::BLOCK_SIZE/FCB::DISK_SIZE;index++) {
            FCB entry;
            if (!readEntry(block,index,entry)) {
                return false;
            }
            result.push_back({entry,block,index});
        }
    }
    entries.swap(result);
    lastError="";
    return true;
}

bool FileSystem::checkTree() {
    if (currentPath.empty()) {
        lastError="file system is not opened";
        return false;
    }
    // [讲解] 临时遍历整棵可达目录树。used只记录本次检查的块归属，不是另一份FAT。
    // 根没有自己的FCB，因此用-1表示“不检查根的FCB大小”；子目录必须逐项相符。
    vector<pair<int,int>> pending{{DiskManager::ROOT_BLOCK,-1}};
    set<int> used;
    for (size_t next=0;next<pending.size();next++) {
        const auto [first,size]=pending[next];
        vector<int> blocks;
        if (!disk.getChain(first,blocks)) {
            lastError=disk.getLastError();
            return false;
        }
        if (size!=-1 && static_cast<size_t>(size/DiskManager::BLOCK_SIZE)!=blocks.size()) {
            lastError="directory size does not match its block chain";
            return false;
        }
        for (int block : blocks) {
            if (!used.insert(block).second) {
                lastError="directory cycle or shared disk block detected";
                return false;
            }
        }
        vector<EntryPosition> entries;
        if (!readDirectory(first,entries)) {
            return false;
        }
        set<string> names;
        for (const EntryPosition& position : entries) {
            const FCB& entry=position.entry;
            if (entry.type==0) {
                continue;
            }
            if (!isValidName(entry.name)) {
                return false;
            }
            if (!names.insert(entry.name).second) {
                lastError="duplicate names in a directory";
                return false;
            }
            if (entry.type==2) {
                pending.push_back({entry.firstBlock,entry.size});
                continue;
            }
            if (!disk.getChain(entry.firstBlock,blocks)) {
                lastError=disk.getLastError();
                return false;
            }
            // 不用(size+1023)/1024，避免输入接近INT_MAX时先加法溢出。
            int count=entry.size/DiskManager::BLOCK_SIZE+(entry.size%DiskManager::BLOCK_SIZE!=0);
            if (blocks.size()!=static_cast<size_t>(count)) {
                lastError="file size does not match its block chain";
                return false;
            }
            for (int block : blocks) {
                if (!used.insert(block).second) {
                    lastError="files or directories share disk blocks";
                    return false;
                }
            }
        }
    }
    lastError="";
    return true;
}

/* [剩余功能二：创建]
 * MD和MK共用相同的查重、找槽、空间检查流程；区别只是type和所需块数。
 * 先完成所有参数/空间检查，再写盘；创建0字节文件不调用allocateBlocks。
 */
bool FileSystem::createEntry(const string &name,int sizeBytes,char type) {
    if (!isValidName(name)) {
        return false;
    }
    if (sizeBytes<0) {
        lastError="file size must not be negative";
        return false;
    }
    if (!checkTree()) {
        return false;
    }
    vector<EntryPosition> entries;
    int directory=currentPath.back().firstBlock;
    if (!readDirectory(directory,entries)) {
        return false;
    }
    int freeIndex=-1;
    for (size_t index=0;index<entries.size();index++) {
        if (entries[index].entry.type!=0 && entries[index].entry.name==name) {
            lastError="name already exists in current directory";
            return false;
        }
        if (entries[index].entry.type==0 && freeIndex==-1) {
            freeIndex=static_cast<int>(index);
        }
    }
    int count=sizeBytes/DiskManager::BLOCK_SIZE+(sizeBytes%DiskManager::BLOCK_SIZE!=0);
    bool expand=freeIndex==-1;
    if (disk.getFreeBlockCount()<count+(expand ? 1 : 0)) {
        lastError="not enough free blocks for entry and directory expansion";
        return false;
    }
    string datetime=makeDateTime();
    if (datetime.empty()) {
        lastError="cannot obtain creation time";
        return false;
    }

    EntryPosition slot{};
    if (!expand) {
        slot=entries[static_cast<size_t>(freeIndex)];
    } else {
        // 子目录大小记录在“父目录的FCB”中，先找到它，不能扩容之后才发现找不到。
        EntryPosition parent{};
        if (currentPath.size()>1) {
            vector<EntryPosition> siblings;
            if (!readDirectory(currentPath[currentPath.size()-2].firstBlock,siblings)) {
                return false;
            }
            bool found=false;
            for (const EntryPosition& position : siblings) {
                if (position.entry.type==2 && position.entry.name==currentPath.back().name &&
                    position.entry.firstBlock==directory) {
                    parent=position;
                    found=true;
                    break;
                }
            }
            if (!found) {
                lastError="current directory is missing from its parent";
                return false;
            }
        }
        if (!disk.extendChain(directory,1)) {
            lastError=disk.getLastError();
            return false;
        }
        vector<int> blocks;
        if (!disk.getChain(directory,blocks)) {
            lastError=disk.getLastError();
            return false;
        }
        slot={FCB{},blocks.back(),0};
        vector<char> zeros(DiskManager::BLOCK_SIZE,0);
        if (!disk.writeBlock(slot.blockNo,zeros)) {
            lastError="directory extended but initialization failed: "+disk.getLastError();
            return false;
        }
        if (currentPath.size()>1) {
            parent.entry.size+=DiskManager::BLOCK_SIZE;
            if (!writeEntry(parent.blockNo,parent.entryIndex,parent.entry)) {
                lastError="directory extended but parent size update failed: "+lastError;
                return false;
            }
        }
    }

    int first=-1;
    if (count>0) {
        if (!disk.allocateBlocks(count,first)) {
            lastError=disk.getLastError();
            return false;
        }
        vector<int> blocks;
        if (!disk.getChain(first,blocks)) {
            lastError=disk.getLastError();
            return false;
        }
        vector<char> zeros(DiskManager::BLOCK_SIZE,0);
        for (int block : blocks) {
            if (!disk.writeBlock(block,zeros)) {
                string error=disk.getLastError();
                if (!disk.freeChain(first)) {
                    error+="; cleanup failed: "+disk.getLastError();
                }
                lastError=error;
                return false;
            }
        }
    }
    FCB created{name,sizeBytes,first,type,datetime};
    if (!writeEntry(slot.blockNo,slot.entryIndex,created)) {
        string error=lastError;
        // 尽力恢复原槽位后才释放新链；若恢复也失败，保留链，避免释放可能仍被引用的块。
        if (!writeEntry(slot.blockNo,slot.entryIndex,slot.entry)) {
            error+="; entry restore failed: "+lastError;
        } else if (!disk.freeChain(first)) {
            error+="; cleanup failed: "+disk.getLastError();
        }
        lastError=error;
        return false;
    }
    lastError="";
    return true;
}

bool FileSystem::makeDirectory(const string &name) {
    return createEntry(name,DiskManager::BLOCK_SIZE,2);
}

bool FileSystem::makeFile(const string &name,int sizeBytes) {
    return createEntry(name,sizeBytes,1);
}

bool FileSystem::changeDirectory(const string &target) {
    if (!checkTree()) {
        return false;
    }
    if (target=="\\") {
        currentPath.resize(1);
    } else if (target=="..") {
        if (currentPath.size()>1) {
            currentPath.pop_back();
        }
    } else {
        if (!isValidName(target)) {
            return false;
        }
        vector<EntryPosition> entries;
        if (!readDirectory(currentPath.back().firstBlock,entries)) {
            return false;
        }
        bool found=false;
        for (const EntryPosition& position : entries) {
            if (position.entry.type!=0 && position.entry.name==target) {
                if (position.entry.type!=2) {
                    lastError="target is a file, not a directory";
                    return false;
                }
                currentPath.push_back({position.entry.name,position.entry.firstBlock});
                found=true;
                break;
            }
        }
        if (!found) {
            lastError="directory not found";
            return false;
        }
    }
    lastError="";
    return true;
}

/* [剩余功能三：删除]
 * RD必须先遍历子目录的全部块，确认没有有效项；不能只检查第一块。
 * DEL/RD先使目录项失效，再回收整条链；回收失败时尝试恢复原目录项。
 * 这些是尽力恢复，不是断电安全的事务；实际I/O失败可能已经部分写入。
 */
bool FileSystem::removeEntry(const string &name,char type) {
    if (!isValidName(name) || !checkTree()) {
        return false;
    }
    vector<EntryPosition> entries;
    if (!readDirectory(currentPath.back().firstBlock,entries)) {
        return false;
    }
    for (const EntryPosition& position : entries) {
        if (position.entry.type==0 || position.entry.name!=name) {
            continue;
        }
        if (position.entry.type!=type) {
            lastError=type==1 ? "DEL cannot delete a directory" : "RD cannot delete a file";
            return false;
        }
        if (type==2) {
            vector<EntryPosition> children;
            if (!readDirectory(position.entry.firstBlock,children)) {
                return false;
            }
            for (const EntryPosition& child : children) {
                if (child.entry.type!=0) {
                    lastError="directory is not empty";
                    return false;
                }
            }
        }
        FCB deleted=position.entry;
        deleted.type=0;
        if (!writeEntry(position.blockNo,position.entryIndex,deleted)) {
            string error=lastError;
            if (!writeEntry(position.blockNo,position.entryIndex,position.entry)) {
                error+="; entry restore failed: "+lastError;
            }
            lastError=error;
            return false;
        }
        if (!disk.freeChain(position.entry.firstBlock)) {
            string error=disk.getLastError();
            if (!writeEntry(position.blockNo,position.entryIndex,position.entry)) {
                error+="; entry restore failed: "+lastError;
            }
            lastError=error;
            return false;
        }
        lastError="";
        return true;
    }
    lastError="entry not found";
    return false;
}

bool FileSystem::removeDirectory(const string &name) {
    return removeEntry(name,2);
}

bool FileSystem::deleteFile(const string &name) {
    return removeEntry(name,1);
}

/* [剩余功能四：显示]
 * DIR先读完并校验，再拼好文本输出，避免中途读盘失败时显示半张目录。
 * 使用局部ostringstream排版，不把setw/setfill等状态遗留给主菜单。
 */
bool FileSystem::listDirectory() {
    if (!checkTree()) {
        return false;
    }
    vector<EntryPosition> entries;
    if (!readDirectory(currentPath.back().firstBlock,entries)) {
        return false;
    }
    ostringstream output;
    for (const EntryPosition& position : entries) {
        const FCB& entry=position.entry;
        if (entry.type==0) {
            continue;
        }
        output<<entry.datetime.substr(0,4)<<'-'<<entry.datetime.substr(4,2)<<'-'
              <<entry.datetime.substr(6,2)<<"  "<<entry.datetime.substr(9,2)<<':'
              <<entry.datetime.substr(11,2)<<':'<<entry.datetime.substr(13,2)<<"  ";
        if (entry.type==2) {
            output<<setw(12)<<"<DIR>";
        } else {
            output<<setw(12)<<entry.size;
        }
        output<<"  "<<entry.name<<'\n';
    }
    string text=output.str();
    cout.write(text.data(),static_cast<streamsize>(text.size()));
    if (!cout) {
        lastError="cannot display directory";
        return false;
    }
    lastError="";
    return true;
}

bool FileSystem::showDiskInfo(int byteOffset,int byteCount) {
    if (!disk.showInfo(byteOffset,byteCount)) {
        lastError=disk.getLastError();
        return false;
    }
    lastError="";
    return true;
}

string FileSystem::getCurrentPath() const {
    if (currentPath.empty()) {
        return "";
    }
    string path="\\";
    for (size_t index=1;index<currentPath.size();index++) {
        path+=currentPath[index].name+"\\";
    }
    return path;
}

const string &FileSystem::getLastError() const {
    return lastError;
}
