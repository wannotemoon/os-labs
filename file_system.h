#ifndef OS_FILE_SYSTEM_H
#define OS_FILE_SYSTEM_H

#include <string>
#include <vector>
#include "disk.h"
#include "fcb.h"
using namespace std;

class FileSystem {
private:
    // 只保存当前导航路径，目录树本身保存在虚盘中。
    struct DirectoryPosition {
        string name;
        int firstBlock;
    };

    DiskManager disk;
    vector<DirectoryPosition> currentPath;
    string lastError;

    // 临时的目录项位置，不另外保存一棵内存目录树；也包含type=0的可复用槽位。
    struct EntryPosition {
        FCB entry;
        int blockNo;
        int entryIndex;
    };

    bool readDirectory(int firstBlock,vector<EntryPosition>& entries);
    // 从根检查所有可达目录：拒绝循环、交叉占块、重名和FCB大小/链长度不一致。
    // 不修复磁盘，也不把无主的已分配块擅自当作空闲块。
    bool checkTree();
    bool createEntry(const string& name,int sizeBytes,char type);
    bool removeEntry(const string& name,char type);

    // 名称区分大小写；拒绝空白、控制字符、分隔符、保留标点、.、..及超过8字节。
    bool isValidName(const string& name);
    string makeDateTime() const;

    // entryIndex为块内0至31号槽位；固定32字节，按小端显式编解码。
    // 只依赖成员disk已打开，不要求currentPath就绪；不负责查找名称或验证FAT归属。
    // 读取失败保留原entry；类型0成功输出默认无效FCB，不解析残留旧字段。
    bool readEntry(int blockNo,int entryIndex,FCB& entry);

    // 活动项校验固定字段格式；只覆盖指定槽位，名称不足8字节补0，日期固定15字节。
    // type=0只修改类型字节，保留其他旧字段；不改输入entry、FAT或其他槽位。
    // 写入中途I/O失败可能留下部分磁盘修改，不承诺事务回滚。
    bool writeEntry(int blockNo,int entryIndex,const FCB& entry);

public:
    // 只准备管理对象；open成功前不执行目录操作。
    explicit FileSystem(int blockCount=DiskManager::DEFAULT_BLOCK_COUNT);

    // 打开/新建虚盘，成功后当前目录设为根；首次失败时不进入命令循环。
    // 同一对象已打开时拒绝重复open，失败保留原导航；不做整盘目录/块链完整性扫描。
    bool open(const string& diskPath);

    // MD：在当前目录创建空子目录，初始分配1个块，大小记1024字节。
    // 创建前检查可达目录树，目录槽位不足时扩展一块，并更新父目录记录的大小。
    bool makeDirectory(const string& name);

    // CD：进入子目录，或用..返回上级、用反斜杠回根；失败路径不变。
    bool changeDirectory(const string& target);

    // RD：只删除当前目录中的空子目录，不删除文件或根目录。
    bool removeDirectory(const string& name);

    // MK：创建指定字节大小的文件；大小可为0，同目录中名称不能重复。
    bool makeFile(const string& name,int sizeBytes);

    // DEL：只删除普通文件，回收全部块并使其目录项失效。
    bool deleteFile(const string& name);

    // DIR：仅输出有效目录项，格式按讲义图4-1；读盘失败返回false。
    bool listDirectory();

    // info：转交DiskManager查看整文件字节，不改变当前目录和磁盘。
    bool showDiskInfo(int byteOffset,int byteCount=256);

    // 返回根路径或带尾部反斜杠的当前路径；尚未open时返回空字符串。
    string getCurrentPath() const;

    // bool方法失败时记录原因，成功时清除旧错误；业务错误由命令入口显示。
    const string& getLastError() const;
};

#endif //OS_FILE_SYSTEM_H
