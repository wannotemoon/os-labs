#include "banker.h"
#include <limits>
#include <utility>

using namespace std;

bool Banker::initialize(const ResourceVector& values,const ResourceMatrix& claims,
                        const ResourceMatrix& assigned,string& error) {
    error.clear();
    if (values.empty() || claims.empty() || assigned.size()!=claims.size()
        || values.size()>static_cast<size_t>(numeric_limits<int>::max())
        || claims.size()>static_cast<size_t>(numeric_limits<int>::max())) {
        error="进程/资源数量须为正，矩阵行数须一致";
        return false;
    }
    ResourceVector total=values;
    for (long long value : values) {
        if (value<0) { error="可用资源不能为负数"; return false; }
    }
    for (size_t i=0;i<claims.size();i++) {
        if (claims[i].size()!=values.size() || assigned[i].size()!=values.size()) {
            error="矩阵每行的资源种类数必须一致";
            return false;
        }
        for (size_t j=0;j<values.size();j++) {
            if (assigned[i][j]<0 || claims[i][j]<assigned[i][j]) {
                error="资源数须非负，且Allocation不能超过Max";
                return false;
            }
            if (assigned[i][j]>numeric_limits<long long>::max()-total[j]) {
                error="系统资源总量超出long long上界";
                return false;
            }
            total[j]+=assigned[i][j];
        }
    }
    for (const auto& row : claims) {
        for (size_t j=0;j<values.size();j++) {
            if (row[j]>total[j]) {
                error="单个进程的Max不能超过系统该类资源总量";
                return false;
            }
        }
    }
    // 临时对象先复制成功，再整体替换，初始化失败不会破坏已载入的案例。
    Banker prepared;
    prepared.available=values;
    prepared.maximum=claims;
    prepared.allocation=assigned;
    *this=move(prepared);
    return true;
}

int Banker::processCount() const { return static_cast<int>(maximum.size()); }
int Banker::resourceCount() const { return static_cast<int>(available.size()); }
const ResourceVector& Banker::getAvailable() const { return available; }
const ResourceMatrix& Banker::getMaximum() const { return maximum; }
const ResourceMatrix& Banker::getAllocation() const { return allocation; }

ResourceMatrix Banker::getNeed() const {
    ResourceMatrix need=maximum;
    for (size_t i=0;i<need.size();i++) {
        for (size_t j=0;j<available.size();j++) need[i][j]-=allocation[i][j];
    }
    return need;
}

bool Banker::checkSafety(vector<int>& sequence,vector<SafetyStep>* steps) const {
    sequence.clear();
    if (steps!=nullptr) steps->clear();
    if (maximum.empty()) return false;
    ResourceVector work=available;
    vector<bool> finish(maximum.size(),false);
    const ResourceMatrix need=getNeed();
    while (sequence.size()<maximum.size()) {
        bool progress=false;
        for (size_t i=0;i<maximum.size();i++) {
            if (finish[i]) continue;
            bool enough=true;
            // 向量比较是每一类资源都满足，不是比较资源总和，也不是字典序比较。
            for (size_t j=0;j<work.size();j++) {
                if (need[i][j]>work[j]) { enough=false; break; }
            }
            if (!enough) continue;
            const ResourceVector before=work;
            // 假想先借出Need再全部收回，净增的仅为该进程原来持有的Allocation。
            for (size_t j=0;j<work.size();j++) work[j]+=allocation[i][j];
            finish[i]=true;
            sequence.push_back(static_cast<int>(i));
            if (steps!=nullptr) steps->push_back({static_cast<int>(i),before,allocation[i],work});
            progress=true;
        }
        if (!progress) return false; // 一整轮都找不到可完成者，不能继续空转。
    }
    return true;
}

RequestResult Banker::requestResources(int processId,const ResourceVector& request,vector<int>& sequence,
                                      vector<SafetyStep>* steps) {
    sequence.clear();
    if (steps!=nullptr) steps->clear();
    if (processId<0 || processId>=processCount() || request.size()!=available.size()) {
        return RequestResult::INVALID_REQUEST;
    }
    for (long long value : request) if (value<0) return RequestResult::INVALID_REQUEST;
    for (size_t j=0;j<request.size();j++) {
        if (request[j]>maximum[processId][j]-allocation[processId][j]) return RequestResult::EXCEEDS_NEED;
    }
    for (size_t j=0;j<request.size();j++) {
        if (request[j]>available[j]) return RequestResult::INSUFFICIENT_AVAILABLE;
    }

    // 在副本上试分配；不安全时直接丢弃副本，原表连临时修改都没有，免去遗漏回滚的风险。
    Banker trial=*this;
    for (size_t j=0;j<request.size();j++) {
        trial.available[j]-=request[j];
        trial.allocation[processId][j]+=request[j];
    }
    vector<int> candidate;
    if (!trial.checkSafety(candidate,steps)) return RequestResult::UNSAFE;
    *this=move(trial);
    sequence=move(candidate);
    return RequestResult::GRANTED;
}
