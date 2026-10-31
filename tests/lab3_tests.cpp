// 只通过正式接口操作，再独立读取虚盘字节核对，不替换算法或开放private。
#include "../file_system.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>

namespace fs=std::filesystem;
int passed=0,failed=0;

void check(bool result,const string& description) {
    (result ? passed : failed)++;
    if (!result) cout<<"FAIL: "<<description<<'\n';
}

vector<char> bytes(const fs::path& path) {
    ifstream input(path,ios::binary);
    if (!input) throw runtime_error("cannot read test disk");
    return vector<char>(istreambuf_iterator<char>(input),{});
}

unsigned field(const vector<char>& data,size_t start,int width) {
    unsigned result=0;
    for (int index=0;index<width;index++) {
        result|=static_cast<unsigned>(static_cast<unsigned char>(data.at(start+index)))<<(index*8);
    }
    return result;
}

void put(vector<char>& data,size_t start,unsigned value,int width) {
    for (int index=0;index<width;index++) data.at(start+index)=static_cast<char>((value>>(index*8))&255);
}

int freeBlocks(const fs::path& path,int count) {
    const auto data=bytes(path);
    int result=0;
    for (int block=0;block<count;block++) result+=field(data,static_cast<size_t>(block*2),2)==0;
    return result;
}

string listing(FileSystem& system,bool& ok) {
    ostringstream output;
    auto* previous=cout.rdbuf(output.rdbuf());
    ok=system.listDirectory();
    cout.rdbuf(previous);
    return output.str();
}

void unchanged(bool result,const fs::path& path,const vector<char>& before,const string& description) {
    check(!result,description+" rejected");
    check(bytes(path)==before,description+" leaves all bytes unchanged");
}

void lectureCase(const fs::path& path,int capacity) {
    {
        FileSystem system(capacity);
        check(system.getCurrentPath().empty(),"path empty before open");
        check(!system.makeDirectory("early") && !system.makeFile("early",0)
              && !system.changeDirectory("\\") && !system.deleteFile("early")
              && !system.removeDirectory("early"),"unopened operations refused");
        bool ok=false;
        check(listing(system,ok).empty() && !ok,"unopened DIR refused");
        check(system.open(path.string()) && system.getCurrentPath()=="\\","new disk opens at root");
        check(system.makeDirectory("test"),"lecture MD test");
        check(system.makeFile("a.c",2000),"lecture MK a.c 2000");
        auto data=bytes(path);
        check(field(data,0,2)==65535 && field(data,2,2)==65535
              && field(data,4,2)==3 && field(data,6,2)==65535,"lecture FAT 0,1,2->3");
        const size_t root=static_cast<size_t>(capacity*2);
        check(field(data,root+8,4)==1024 && field(data,root+12,4)==1 && data[root+16]==2,
              "test FCB layout");
        check(field(data,root+32+8,4)==2000 && field(data,root+32+12,4)==2 && data[root+32+16]==1,
              "a.c FCB layout");
        const auto view=listing(system,ok);
        check(ok && view.find("<DIR>")!=string::npos && view.find("test")!=string::npos
              && view.find("2000")!=string::npos && view.find("a.c")!=string::npos,"DIR lecture format");
        check(view.size()>19 && view[4]=='-' && view[7]=='-' && view[14]==':' && view[17]==':',"DIR readable timestamp");
        auto before=bytes(path);
        unchanged(system.deleteFile("test"),path,before,"DEL directory");
        unchanged(system.removeDirectory("a.c"),path,before,"RD file");
        unchanged(system.makeFile("test",1),path,before,"duplicate cross-type name");
        check(system.changeDirectory("test") && system.getCurrentPath()=="\\test\\","CD test");
        check(system.makeFile("empty",0),"empty file creation");
        check(freeBlocks(path,capacity)==capacity-4,"empty file allocates no block");
        check(system.changeDirectory("..") && system.getCurrentPath()=="\\","CD parent");
        before=bytes(path);
        unchanged(system.removeDirectory("test"),path,before,"RD nonempty directory");
        check(!system.changeDirectory("a.c") && system.getCurrentPath()=="\\","CD file preserves path");
        check(system.changeDirectory("test") && system.deleteFile("empty") && system.changeDirectory("\\"),
              "remove empty file and return root");
        check(system.removeDirectory("test"),"lecture RD empty test");
        data=bytes(path);
        check(field(data,2,2)==0 && data[root+16]==0 && data[root]=='t',"RD clears FAT/type but retains old name bytes");
        check(system.makeDirectory("test") && system.changeDirectory("test"),"reuse freed directory block");
        check(listing(system,ok).empty() && ok,"reused directory starts empty");
        check(system.changeDirectory("\\") && system.removeDirectory("test"),"root reset and RD");
        check(system.getLastError().empty(),"successful bool clears old error");
    }
    {
        FileSystem reopened(capacity);
        const auto before=bytes(path);
        check(reopened.open(path.string()) && reopened.getCurrentPath()=="\\","reopen root");
        bool ok=false;
        const auto view=listing(reopened,ok);
        check(ok && view.find("a.c")!=string::npos && view.find("test")==string::npos,"reopen preserves active/deleted entries");
        check(bytes(path)==before,"reopening/listing does not write");
        check(reopened.deleteFile("a.c") && freeBlocks(path,capacity)==capacity-1,"DEL releases every file block");
        check(reopened.changeDirectory("..") && reopened.getCurrentPath()=="\\","CD .. at root stays root");
        check(!reopened.removeDirectory("\\") && !reopened.removeDirectory("..") && !reopened.removeDirectory("."),"root/reserved deletion refused");
    }
}

void expansion(const fs::path& path) {
    FileSystem system(8);
    check(system.open(path.string()),"expansion disk opens");
    for (int index=0;index<33;index++) check(system.makeFile("r"+to_string(index),0),"root directory fill/extend");
    auto data=bytes(path);
    check(field(data,0,2)==1 && field(data,2,2)==65535 && freeBlocks(path,8)==6,"root extends to second block");
    check(system.deleteFile("r0"),"delete first root slot");
    check(system.makeFile("reuse",0) && freeBlocks(path,8)==6,"reuse slot before expanding");
    check(system.makeDirectory("child") && system.changeDirectory("child"),"create child for expansion");
    for (int index=0;index<65;index++) check(system.makeFile("c"+to_string(index),0),"child fill/extend twice");
    check(system.changeDirectory("\\"),"return root after expansion");
    data=bytes(path);
    bool recorded=false;
    for (int block : {0,1}) {
        for (int index=0;index<32;index++) {
            size_t offset=16+static_cast<size_t>(block*1024+index*32);
            if (data[offset+16]==2) recorded=field(data,offset+8,4)==3072;
        }
    }
    check(recorded,"child size updated in parent FCB to 3072");
    const auto before=bytes(path);
    unchanged(system.removeDirectory("child"),path,before,"RD expanded nonempty child");
    check(system.changeDirectory("child"),"reenter expanded child");
    for (int index=0;index<65;index++) check(system.deleteFile("c"+to_string(index)),"remove all expanded child entries");
    check(system.changeDirectory("\\") && system.removeDirectory("child"),"RD releases empty expanded directory");
    check(freeBlocks(path,8)==6,"expanded child chain fully reclaimed; root capacity retained");
}

void boundaries(const fs::path& path) {
    FileSystem system(8);
    check(system.open(path.string()),"boundary disk opens");
    const auto initial=bytes(path);
    vector<string> names={"",".","..","123456789","a b","a\tb","a/b","a\\b","a:b","a?b","a*b","a\"b","a<b","a>b","a|b",string("a\0b",3),string(1,char(127))};
    for (const string& name : names) {
        unchanged(system.makeFile(name,0),path,initial,"invalid file name");
        unchanged(system.makeDirectory(name),path,initial,"invalid directory name");
    }
    for (int size : {-1,std::numeric_limits<int>::min(),std::numeric_limits<int>::max(),7169}) {
        unchanged(system.makeFile("bad",size),path,initial,"invalid or too large size");
    }
    check(system.makeFile("12345678",0) && system.makeFile("Ab",0) && system.makeFile("ab",0),"eight-byte and case-sensitive names");
    check(system.makeFile("中文a",0),"UTF-8 byte-count name");
    check(system.makeFile("full",7168) && freeBlocks(path,8)==0,"exactly fill disk");
    auto before=bytes(path);
    unchanged(system.makeFile("one",1),path,before,"full disk rejects nonempty file");
    unchanged(system.makeDirectory("dir"),path,before,"full disk rejects directory");
    check(system.makeFile("zero",0),"full data disk accepts empty file in existing slot");
    check(system.deleteFile("full") && freeBlocks(path,8)==7,"recover full-disk file");
    // 剩余一个空闲块，但目录扩展与新文件共需两个，必须在任何写入前拒绝。
    check(system.makeFile("big",6144),"leave one free block");
    for (int index=0;index<26;index++) check(system.makeFile("x"+to_string(index),0),"fill root slots without allocating");
    before=bytes(path);
    unchanged(system.makeFile("extra",1),path,before,"preflight includes directory expansion block");
    check(system.makeFile("extra",0) && freeBlocks(path,8)==0,"last block can expand directory for empty file");
    check(system.deleteFile("big"),"free six blocks after expansion");
}

void corrupted(const fs::path& folder) {
    const auto base=folder/"corruption-base.disk";
    { FileSystem system(8); check(system.open(base.string()) && system.makeDirectory("test")
          && system.makeFile("a.c",2000),"corruption baseline"); }
    const auto original=bytes(base);
    vector<vector<char>> variants;
    auto data=original; put(data,4,2,2); variants.push_back(data); // 文件链自环
    data=original; put(data,16+32+8,1,4); variants.push_back(data); // size/chain不匹配
    data=original; put(data,16+32+12,1,4); variants.push_back(data); // 文件与目录交叉占块
    data=original; data[16+16]=3; variants.push_back(data); // FCB类型错误
    data=original; put(data,16+8,2048,4); variants.push_back(data); // 目录大小错误
    data=original; put(data,16+1024+8,1024,4); put(data,16+1024+12,1,4);
    data[16+1024]='x'; data[16+1024+16]=2;
    std::copy(original.begin()+16+17,original.begin()+16+32,data.begin()+16+1024+17);
    variants.push_back(data); // test目录指向自己
    data=original; data[16+32]='t'; data[16+33]='e'; data[16+34]='s'; data[16+35]='t'; variants.push_back(data);
    int index=0;
    for (const auto& variant : variants) {
        const auto path=folder/("corrupt-"+to_string(index++)+".disk");
        if (fs::exists(path)) throw runtime_error("test refuses existing fixture");
        { ofstream output(path,ios::binary); output.write(variant.data(),static_cast<streamsize>(variant.size())); }
        FileSystem system(8);
        check(system.open(path.string()),"structural corruption passes only shallow open");
        bool ok=true;
        check(listing(system,ok).empty() && !ok,"corrupted tree refused without partial DIR");
        unchanged(system.deleteFile("a.c"),path,variant,"corrupt tree DEL");
        unchanged(system.makeFile("new",0),path,variant,"corrupt tree MK");
        unchanged(system.removeDirectory("test"),path,variant,"corrupt tree RD");
        check(!system.changeDirectory("test") && system.getCurrentPath()=="\\","corrupt tree CD path unchanged");
    }
}

struct ModelEntry { bool directory; int size; };
struct ModelDirectory { map<string,ModelEntry> entries; int capacity=32; };

bool matchesListing(FileSystem& system,const map<string,ModelEntry>& expected) {
    bool ok=false;
    const string text=listing(system,ok);
    if (!ok) return false;
    istringstream rows(text);
    string row;
    map<string,ModelEntry> actual;
    while (getline(rows,row)) {
        istringstream columns(row);
        string date,clock,size,name,extra;
        if (!(columns>>date>>clock>>size>>name) || (columns>>extra) || actual.count(name)!=0) return false;
        int number=0;
        if (size!="<DIR>") {
            istringstream integer(size);
            if (!(integer>>number) || (integer>>extra)) return false;
        }
        actual[name]={size=="<DIR>",number};
    }
    if (actual.size()!=expected.size()) return false;
    for (const auto& [name,entry] : expected) {
        const auto found=actual.find(name);
        if (found==actual.end() || found->second.directory!=entry.directory
            || (!entry.directory && found->second.size!=entry.size)) return false;
    }
    return true;
}

void modelTest(const fs::path& path) {
    FileSystem system(16);
    check(system.open(path.string()),"model disk opens");
    map<string,ModelDirectory> tree{{"\\",{}}};
    vector<string> navigation{"\\"};
    int free=15;
    mt19937 random(20261023);
    for (int step=0;step<1000;step++) {
        const string current=navigation.back();
        auto& directory=tree.at(current);
        const string name="n"+to_string(random()%10);
        const int operation=static_cast<int>(random()%6);
        const int size=static_cast<int>(random()%3500);
        auto existing=directory.entries.find(name);
        const bool present=existing!=directory.entries.end();
        bool expected=false,actual=false;
        const auto before=bytes(path);
        if (operation==0 || operation==1) {
            const int required=operation==0 ? 1 : size/1024+(size%1024!=0);
            const bool expand=static_cast<int>(directory.entries.size())==directory.capacity;
            expected=!present && free>=required+(expand ? 1 : 0);
            actual=operation==0 ? system.makeDirectory(name) : system.makeFile(name,size);
            if (expected) {
                free-=required+(expand ? 1 : 0);
                if (expand) directory.capacity+=32;
                directory.entries[name]={operation==0,operation==0 ? 1024 : size};
                if (operation==0) tree[current+name+"\\"]={};
            }
        } else if (operation==2 || operation==3) {
            expected=present && existing->second.directory==(operation==2);
            const string child=current+name+"\\";
            if (expected && operation==2) expected=tree.at(child).entries.empty();
            actual=operation==2 ? system.removeDirectory(name) : system.deleteFile(name);
            if (expected) {
                if (operation==2) {
                    free+=tree.at(child).capacity/32;
                    tree.erase(child);
                } else {
                    const int fileSize=existing->second.size;
                    free+=fileSize/1024+(fileSize%1024!=0);
                }
                directory.entries.erase(name);
            }
        } else if (operation==4) {
            expected=present && existing->second.directory;
            actual=system.changeDirectory(name);
            if (expected) navigation.push_back(current+name+"\\");
        } else {
            expected=true;
            actual=system.changeDirectory("..");
            if (navigation.size()>1) navigation.pop_back();
        }
        check(actual==expected,"model operation "+to_string(step));
        check(system.getCurrentPath()==navigation.back(),"model path "+to_string(step));
        check(freeBlocks(path,16)==free,"model free blocks "+to_string(step));
        check(matchesListing(system,tree.at(navigation.back()).entries),"model directory contents "+to_string(step));
        if (!expected) check(bytes(path)==before,"failed model operation writes nothing");
        if (actual!=expected) break; // 防止首次不一致后让参考模型继续产生误导性结果。
    }
}

void fragmentation(const fs::path& path) {
    {
        FileSystem system(8);
        check(system.open(path.string()),"fragment disk opens");
        for (int index=0;index<6;index++) check(system.makeFile("f"+to_string(index),1024),"prepare fragment files");
    }
    // 给测试磁盘的数据区填入非零旧内容，再重开；目录块和FAT不动。
    {
        fstream output(path,ios::in|ios::out|ios::binary);
        vector<char> content(6*1024,static_cast<char>(0xA5));
        output.seekp(16+1024,ios::beg);
        output.write(content.data(),static_cast<streamsize>(content.size()));
        output.flush();
        check(static_cast<bool>(output),"inject stale bytes into test file data");
    }
    FileSystem system(8);
    check(system.open(path.string()) && system.deleteFile("f1") && system.deleteFile("f3")
          && system.deleteFile("f5"),"create fragmented free blocks 2,4,6,7");
    check(system.makeFile("frag",2500),"allocate fragmented three-block file");
    const auto data=bytes(path);
    check(field(data,4,2)==4 && field(data,8,2)==6 && field(data,12,2)==65535,"fragment FAT 2->4->6");
    for (int block : {2,4,6}) {
        const auto start=data.begin()+16+block*1024;
        check(all_of(start,start+1024,[](char ch) { return ch==0; }),"new file blocks initialized to zero");
    }
    for (int block : {1,3,5}) {
        const auto start=data.begin()+16+block*1024;
        check(all_of(start,start+1024,[](char ch) { return static_cast<unsigned char>(ch)==0xA5; }),"other file content preserved");
    }
    check(system.deleteFile("frag") && freeBlocks(path,8)==4,"fragmented full chain reclaimed");
    check(system.makeDirectory("clean") && system.changeDirectory("clean"),"reused old file block becomes directory");
    bool ok=false;
    check(listing(system,ok).empty() && ok,"reused directory contains no stale entries");
}

int main(int argc,char** argv) {
    try {
        fs::path parent=argc>1 ? fs::path(argv[1]) : fs::current_path();
        fs::create_directories(parent);
        const auto folder=parent/("run-"+to_string(chrono::high_resolution_clock::now().time_since_epoch().count()));
        if (!fs::create_directory(folder)) throw runtime_error("cannot create unique test folder");
        for (int capacity : {8,64,1024}) lectureCase(folder/("lecture-"+to_string(capacity)+".disk"),capacity);
        expansion(folder/"expansion.disk");
        boundaries(folder/"boundaries.disk");
        corrupted(folder);
        modelTest(folder/"model.disk");
        fragmentation(folder/"fragmentation.disk");
        cout<<"Passed: "<<passed<<" Failed: "<<failed<<'\n';
        return failed==0 ? 0 : 1;
    } catch (const exception& error) {
        cerr<<"Test error: "<<error.what()<<'\n';
        return 2;
    }
}
