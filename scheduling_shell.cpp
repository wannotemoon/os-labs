#include "scheduling_shell.h"
#include "process.h"
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace std;

namespace {
// 实验四统一逐行读输入：12x、小数、超范围整数、额外参数都整行拒绝，不把残余字符留给下一题。
// false只表示遇到EOF；普通错误留在当前提示重输，不改变已经创建的任务。
bool readNumber(const char* prompt,long long minimum,long long maximum,long long& value) {
    while (true) {
        cout<<prompt;
        string line;
        if (!getline(cin,line)) return false;
        istringstream input(line);
        string extra;
        if ((input>>value) && !(input>>extra) && value>=minimum && value<=maximum) return true;
        cout<<"输入无效，请输入"<<minimum<<"到"<<maximum<<"范围内的一个整数"<<endl;
    }
}
}

void runSchedulingMenu() {
    while (true) {
        cout<<endl<<"-------- 实验四：进程调度 --------"<<endl;
        cout<<"1. FCFS（先来先服务）"<<endl;
        cout<<"2. SJF（非抢占式短作业优先）"<<endl;
        cout<<"3. RR（时间片轮转）"<<endl;
        cout<<"0. 返回主菜单"<<endl;
        long long choice;
        if (!readNumber("请选择调度算法：",0,3,choice) || choice==0) return;
        const auto algorithm=choice==1 ? SchedulingAlgorithm::FCFS
                           : choice==2 ? SchedulingAlgorithm::SJF : SchedulingAlgorithm::RR;
        long long quantum=0;
        if (algorithm==SchedulingAlgorithm::RR
            && !readNumber("请输入正整数时间片：",1,numeric_limits<long long>::max(),quantum)) return;
        long long count;
        // 默认64KB物理内存、1KB页面，每个成功创建的进程至少驻留1页，所以一批至多64个。
        if (!readNumber("请输入进程数（1-64）：",1,64,count)) return;

        // 每轮使用自己的管理器，自动调度与主菜单原有的手动进程完全隔离。
        // continue/return离开本轮时析构，取消的任务、失败后尚占用的资源也会统一清理。
        ProcessManager batch;
        bool cancelled=false;
        for (int index=1;index<=count;) {
            cout<<"进程 "<<index<<'/'<<count
                <<"：请输入 名称 大小(字节) 到达时间 服务时间 驻留页数（-1默认，单独输入0取消本批）：";
            string line;
            if (!getline(cin,line)) return;
            istringstream input(line);
            string name,extra;
            if (!(input>>name)) {
                cout<<"输入无效，请填写五项进程信息"<<endl;
                continue;
            }
            if (name=="0") {
                input>>ws;
                if (input.eof()) { cancelled=true; break; }
            }
            int sizeBytes,residentPages;
            long long arrivalTime,burstTime;
            if (!(input>>sizeBytes>>arrivalTime>>burstTime>>residentPages) || (input>>extra)) {
                cout<<"输入无效：格式为 名称 大小 到达时间 服务时间 驻留页数，名称不含空白"<<endl;
                continue;
            }
            // 沿用已确认的批量预分配规则：失败只拒绝当前进程，保留此前成功者并重输本项。
            if (!batch.createScheduledProcess(name,sizeBytes,arrivalTime,burstTime,residentPages)) {
                cout<<"进程创建失败：请检查大小、非负到达时间、正服务时间、驻留页数及空闲空间；可输入0取消本批"<<endl;
                continue;
            }
            cout<<"已加入进程："<<name<<endl;
            ++index;
        }
        if (cancelled) {
            cout<<"已取消本批调度"<<endl;
            continue;
        }

        // 输入结束后才开始运行；完整保留过程快照和完成队列，统计使用已完成者的真实时间。
        if (batch.runScheduling(algorithm,quantum)) {
            cout<<"本批调度完成"<<endl;
        } else {
            cout<<"本批未全部完成：时间可能超出上限或资源状态异常"<<endl;
            batch.showSchedulingState();
        }
        batch.showSchedulingStatistics();
        // 再次选择算法会创建新批次；本轮finished在对象析构前供上面统计，之后统一删除。
    }
}
