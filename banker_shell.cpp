#include "banker_shell.h"
#include "banker.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace {
string resourceText(const ResourceVector& values) {
    ostringstream text;
    text<<'[';
    for (size_t j=0;j<values.size();j++) { if (j!=0) text<<' '; text<<values[j]; }
    text<<']';
    return text.str();
}

bool readNumber(const char* prompt,long long minimum,long long maximum,long long& value) {
    while (true) {
        cout<<prompt;
        string line,extra;
        if (!getline(cin,line)) return false;
        istringstream input(line);
        if ((input>>value) && !(input>>extra) && value>=minimum && value<=maximum) return true;
        cout<<"输入无效，请输入"<<minimum<<"到"<<maximum<<"范围内的整数"<<endl;
    }
}

// q取消当前输入；EOF也返回false，调用者通过cin状态判断是否结束整个输入。
bool readResources(const string& prompt,int count,ResourceVector& values) {
    while (true) {
        cout<<prompt<<"（"<<count<<"个非负整数，q取消）：";
        string line,extra;
        if (!getline(cin,line)) return false;
        istringstream input(line);
        string first;
        if ((input>>first) && first=="q" && !(input>>extra)) return false;
        input.clear();
        input.str(line);
        ResourceVector parsed(static_cast<size_t>(count));
        bool valid=true;
        for (auto& value : parsed) if (!(input>>value) || value<0) { valid=false; break; }
        if (valid && !(input>>extra)) { values=move(parsed); return true; }
        cout<<"输入无效：资源数须非负、数量正确且没有额外参数"<<endl;
    }
}

Banker safeExample() {
    Banker banker;
    string error;
    if (!banker.initialize({3,3,2},
        {{7,5,3},{3,2,2},{9,0,2},{2,2,2},{4,3,3}},
        {{0,1,0},{2,0,0},{3,0,2},{2,1,1},{0,0,2}},error)) throw logic_error(error);
    return banker;
}

Banker unsafeExample() {
    Banker banker;
    string error;
    // P0可以完成；释放后Work=[2 0]，P1和P2仍不能全部获得所需资源。
    if (!banker.initialize({1,0},{{2,0},{3,1},{2,1}},{{1,0},{0,1},{1,0}},error)) throw logic_error(error);
    return banker;
}

void showState(const Banker& banker) {
    cout<<"资源顺序：";
    for (int j=0;j<banker.resourceCount();j++) cout<<'R'<<j<<' ';
    cout<<endl<<"Available: "<<resourceText(banker.getAvailable())<<endl;
    const auto need=banker.getNeed();
    size_t width=10;
    for (int i=0;i<banker.processCount();i++) {
        width=max(width,resourceText(banker.getMaximum()[i]).size());
        width=max(width,resourceText(banker.getAllocation()[i]).size());
        width=max(width,resourceText(need[i]).size());
    }
    const auto oldFlags=cout.flags();
    cout<<left<<setw(7)<<"PID"<<setw(static_cast<int>(width+2))<<"Max"
        <<setw(static_cast<int>(width+2))<<"Allocation"<<"Need"<<endl;
    for (int i=0;i<banker.processCount();i++) {
        cout<<setw(7)<<("P"+to_string(i))<<setw(static_cast<int>(width+2))<<resourceText(banker.getMaximum()[i])
            <<setw(static_cast<int>(width+2))<<resourceText(banker.getAllocation()[i])<<resourceText(need[i])<<endl;
    }
    cout.flags(oldFlags);
}

void showSequence(const vector<int>& sequence) {
    for (size_t i=0;i<sequence.size();i++) { if (i!=0) cout<<" -> "; cout<<'P'<<sequence[i]; }
    cout<<endl;
}

void showSteps(const vector<SafetyStep>& steps) {
    for (const auto& step : steps) {
        cout<<"假想完成P"<<step.processId<<"：Work "<<resourceText(step.workBefore)
            <<" + Allocation "<<resourceText(step.released)
            <<" = "<<resourceText(step.workAfter)<<endl;
    }
}

void showSafety(const Banker& banker) {
    vector<int> sequence;
    vector<SafetyStep> steps;
    const bool safe=banker.checkSafety(sequence,&steps);
    cout<<"安全性检查：Work初值="<<resourceText(banker.getAvailable())<<endl;
    showSteps(steps);
    if (safe) {
        cout<<"系统安全，安全序列：";
        showSequence(sequence);
    } else {
        cout<<"系统不安全：找不到让所有进程完成的安全序列"<<endl;
        if (!sequence.empty()) { cout<<"仅能假想完成的前缀："; showSequence(sequence); }
        cout<<"不安全表示不能保证避免死锁，不等于此刻已经发生死锁"<<endl;
    }
    cout<<"本次检查只做假想推演，实际资源表不变"<<endl;
}

void performRequest(Banker& banker,int pid,const ResourceVector& request) {
    cout<<"请求：P"<<pid<<' '<<resourceText(request)<<endl;
    vector<int> sequence;
    vector<SafetyStep> steps;
    const auto availableBefore=banker.getAvailable();
    const auto result=banker.requestResources(pid,request,sequence,&steps);
    if (result==RequestResult::GRANTED || result==RequestResult::UNSAFE) {
        auto work=availableBefore;
        for (size_t j=0;j<work.size();j++) work[j]-=request[j];
        cout<<"试分配安全性推演：Work初值="<<resourceText(work)<<endl;
        showSteps(steps);
        if (steps.empty()) cout<<"没有进程满足Need<=Work，无法开始安全序列"<<endl;
    }
    switch (result) {
    case RequestResult::GRANTED:
        cout<<"请求获准：试分配后仍安全，已正式分配"<<endl;
        cout<<"批准后的安全序列："; showSequence(sequence);
        break;
    case RequestResult::EXCEEDS_NEED:
        cout<<"请求被拒绝：超过该进程剩余需求Need，资源表不变"<<endl; break;
    case RequestResult::INSUFFICIENT_AVAILABLE:
        cout<<"请求暂缓：当前可用资源不足，资源表不变"<<endl; break;
    case RequestResult::UNSAFE:
        cout<<"请求被拒绝：试分配后不安全，未提交试分配，资源表不变"<<endl; break;
    case RequestResult::INVALID_REQUEST:
        cout<<"请求无效：编号、维度或资源数量不合法，资源表不变"<<endl; break;
    }
    showState(banker);
}

void requestDemos() {
    struct Demo { const char* name; int pid; ResourceVector request; };
    const vector<Demo> demos{{"安全请求",1,{1,0,2}}, {"超过剩余需求",1,{2,0,0}},
                             {"可用资源不足",0,{0,4,0}}, {"试分配不安全",4,{3,3,0}}};
    cout<<"每项演示均从同一初始安全案例开始，不沿用上一项的分配结果"<<endl;
    for (const auto& demo : demos) {
        Banker banker=safeExample();
        cout<<endl<<"-------- 演示："<<demo.name<<" --------"<<endl;
        showState(banker);
        performRequest(banker,demo.pid,demo.request);
    }
}

bool readManual(Banker& banker) {
    long long n,m;
    // 这是演示输入规模上限，不是银行家算法与分页容量之间的关系。
    if (!readNumber("进程数（1-20，0取消）：",0,20,n) || n==0) return false;
    if (!readNumber("资源种类数（1-10，0取消）：",0,10,m) || m==0) return false;
    ResourceVector available;
    ResourceMatrix maximum(static_cast<size_t>(n)),allocation(static_cast<size_t>(n));
    if (!readResources("Available",static_cast<int>(m),available)) return false;
    for (int i=0;i<n;i++) {
        if (!readResources("P"+to_string(i)+" Max",static_cast<int>(m),maximum[i])) return false;
        if (!readResources("P"+to_string(i)+" Allocation",static_cast<int>(m),allocation[i])) return false;
    }
    string error;
    if (!banker.initialize(available,maximum,allocation,error)) {
        cout<<"数据不合法："<<error<<endl;
        return false;
    }
    return true;
}

void session(Banker& banker) {
    showState(banker);
    showSafety(banker);
    while (true) {
        cout<<endl<<"1. 查看资源表  2. 安全性检查  3. 资源请求  0. 返回案例选择"<<endl;
        long long choice;
        if (!readNumber("请选择：",0,3,choice) || choice==0) return;
        if (choice==1) showState(banker);
        else if (choice==2) showSafety(banker);
        else {
            long long pid;
            if (!readNumber("进程编号（0表示P0）：",0,banker.processCount()-1,pid)) return;
            ResourceVector request;
            if (!readResources("Request",banker.resourceCount(),request)) {
                if (!cin) return;
                cout<<"已取消本次请求"<<endl;
                continue;
            }
            performRequest(banker,static_cast<int>(pid),request);
        }
    }
}
}

void runBankerMenu() {
    while (true) {
        cout<<endl<<"-------- 银行家算法（独立资源分配） --------"<<endl;
        cout<<"1. 预置安全案例（5进程、3类资源）"<<endl;
        cout<<"2. 预置不安全案例（可完成部分进程）"<<endl;
        cout<<"3. 批量演示四种资源请求结果"<<endl;
        cout<<"4. 手动输入Available、Max和Allocation"<<endl;
        cout<<"0. 返回主菜单"<<endl;
        long long choice;
        if (!readNumber("请选择案例：",0,4,choice) || choice==0) return;
        if (choice==3) { requestDemos(); continue; }
        Banker banker;
        if (choice==1) banker=safeExample();
        else if (choice==2) banker=unsafeExample();
        else if (!readManual(banker)) {
            if (!cin) return;
            cout<<"本次手动载入未完成，返回案例选择"<<endl;
            continue;
        }
        session(banker);
        if (!cin) return;
    }
}
