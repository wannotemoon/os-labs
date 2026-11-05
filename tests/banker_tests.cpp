#include "../banker.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>

using namespace std;
int passed=0,failed=0;
void check(bool result,const char* description) {
    (result ? passed : failed)++;
    if (!result) cout<<"FAIL: "<<description<<'\n';
}

struct State {
    ResourceVector available;
    ResourceMatrix maximum,allocation,need;
};
State snapshot(const Banker& banker) {
    return {banker.getAvailable(),banker.getMaximum(),banker.getAllocation(),banker.getNeed()};
}
bool same(const Banker& banker,const State& state) {
    return banker.getAvailable()==state.available && banker.getMaximum()==state.maximum
        && banker.getAllocation()==state.allocation && banker.getNeed()==state.need;
}

Banker classic() {
    Banker banker;
    string error;
    check(banker.initialize({3,3,2},{{7,5,3},{3,2,2},{9,0,2},{2,2,2},{4,3,3}},
                           {{0,1,0},{2,0,0},{3,0,2},{2,1,1},{0,0,2}},error),"classic initialization");
    return banker;
}

bool bruteSafe(const ResourceVector& available,const ResourceMatrix& maximum,const ResourceMatrix& allocation) {
    vector<int> permutation(maximum.size());
    iota(permutation.begin(),permutation.end(),0);
    do {
        auto work=available;
        bool possible=true;
        for (int i : permutation) {
            for (size_t j=0;j<work.size();j++) {
                if (maximum[i][j]-allocation[i][j]>work[j]) { possible=false; break; }
            }
            if (!possible) break;
            for (size_t j=0;j<work.size();j++) work[j]+=allocation[i][j];
        }
        if (possible) return true;
    } while (next_permutation(permutation.begin(),permutation.end()));
    return false;
}

void validateTrace(const Banker& banker,bool safe,const vector<int>& sequence,const vector<SafetyStep>& steps) {
    auto work=banker.getAvailable();
    vector<bool> seen(static_cast<size_t>(banker.processCount()),false);
    const auto need=banker.getNeed();
    check(sequence.size()==steps.size(),"trace covers returned sequence/prefix");
    for (size_t index=0;index<steps.size();index++) {
        const auto& step=steps[index];
        check(step.processId>=0 && step.processId<banker.processCount(),"trace process range");
        check(!seen[step.processId],"each process released at most once");
        seen[step.processId]=true;
        check(sequence[index]==step.processId && step.workBefore==work,"trace order and Work before");
        check(step.released==banker.getAllocation()[step.processId],"release equals allocation");
        for (size_t j=0;j<work.size();j++) {
            check(need[step.processId][j]<=work[j],"each resource need fits Work");
            work[j]+=step.released[j];
        }
        check(step.workAfter==work,"trace Work after");
    }
    check(safe ? sequence.size()==seen.size() : sequence.size()<seen.size(),"only safe state has a complete sequence");
}

void fixedCases() {
    Banker empty;
    vector<int> sequence{99};
    vector<SafetyStep> steps;
    check(!empty.checkSafety(sequence,&steps) && sequence.empty() && steps.empty(),"empty model is not a configured safe case");
    check(empty.requestResources(0,{},sequence)==RequestResult::INVALID_REQUEST,"uninitialized request rejected");
    Banker banker=classic();
    const auto before=snapshot(banker);
    check(banker.processCount()==5 && banker.resourceCount()==3,"dimensions");
    check(banker.getNeed()==ResourceMatrix{{7,4,3},{1,2,2},{6,0,0},{0,1,1},{4,3,1}},"Need derived from Max-Allocation");
    check(banker.checkSafety(sequence,&steps),"classic state safe");
    check(sequence==vector<int>{1,3,4,0,2},"deterministic complete safety sequence");
    validateTrace(banker,true,sequence,steps);
    check(same(banker,before),"safety check never changes state");
    string error;
    check(banker.initialize({1,0},{{2,0},{3,1},{2,1}},{{1,0},{0,1},{1,0}},error),"valid unsafe configuration accepted");
    const auto unsafeBefore=snapshot(banker);
    check(!banker.checkSafety(sequence,&steps) && sequence==vector<int>{0},"unsafe state may have feasible prefix");
    validateTrace(banker,false,sequence,steps);
    check(same(banker,unsafeBefore),"unsafe check preserves state too");
    check(banker.initialize({0},{{0},{0}},{{0},{0}},error),"zero resources and zero claims valid");
    check(banker.checkSafety(sequence,&steps),"zero Need tasks can finish with zero Work");
    validateTrace(banker,true,sequence,steps);
}

void initializationGuards() {
    Banker banker=classic();
    const auto before=snapshot(banker);
    const auto reject=[&](const ResourceVector& available,const ResourceMatrix& maximum,const ResourceMatrix& allocation) {
        string error;
        check(!banker.initialize(available,maximum,allocation,error),"invalid configuration rejected");
        check(!error.empty() && same(banker,before),"initialization failure explains error and preserves existing state");
    };
    reject({},{{1}},{{0}});
    reject({1},{},{});
    reject({1},{{1}},{});
    reject({-1},{{1}},{{0}});
    reject({1},{{1,2}},{{0}});
    reject({1},{{1}},{{0,0}});
    reject({1},{{1}},{{-1}});
    reject({1},{{0}},{{1}});
    reject({1},{{-1}},{{0}});
    reject({1},{{2}},{{0}});
    reject({numeric_limits<long long>::max()},{{1}},{{1}});
}

void requestCases() {
    struct Case { int pid; ResourceVector request; RequestResult expected; };
    const vector<Case> cases{{1,{1,0,2},RequestResult::GRANTED}, {4,{3,3,0},RequestResult::UNSAFE},
        {0,{0,4,0},RequestResult::INSUFFICIENT_AVAILABLE}, {1,{2,0,0},RequestResult::EXCEEDS_NEED},
        {-1,{0,0,0},RequestResult::INVALID_REQUEST}, {5,{0,0,0},RequestResult::INVALID_REQUEST},
        {0,{},RequestResult::INVALID_REQUEST}, {0,{0,0},RequestResult::INVALID_REQUEST},
        {0,{0,0,0,0},RequestResult::INVALID_REQUEST}, {0,{-1,0,0},RequestResult::INVALID_REQUEST},
        {0,{numeric_limits<long long>::max(),0,0},RequestResult::EXCEEDS_NEED}, {0,{0,0,0},RequestResult::GRANTED}};
    for (const auto& test : cases) {
        Banker banker=classic();
        const auto before=snapshot(banker);
        vector<int> sequence{99};
        vector<SafetyStep> steps;
        const auto result=banker.requestResources(test.pid,test.request,sequence,&steps);
        check(result==test.expected,"request classification");
        if (result==RequestResult::GRANTED) {
            auto available=before.available;
            auto allocation=before.allocation;
            for (size_t j=0;j<available.size();j++) { available[j]-=test.request[j]; allocation[test.pid][j]+=test.request[j]; }
            check(banker.getAvailable()==available && banker.getAllocation()==allocation && banker.getMaximum()==before.maximum,
                  "only approved request is committed exactly");
            validateTrace(banker,true,sequence,steps);
        } else {
            check(sequence.empty() && same(banker,before),"rejected request leaves all resource fields unchanged");
        }
    }
}

void limits() {
    const auto upper=numeric_limits<long long>::max();
    Banker banker;
    string error;
    check(banker.initialize({upper-1},{{1},{upper}},{{1},{0}},error),"maximum system resource total accepted");
    vector<int> sequence;
    vector<SafetyStep> steps;
    check(banker.checkSafety(sequence,&steps),"Work reaches upper bound without overflow");
    validateTrace(banker,true,sequence,steps);
    check(banker.requestResources(1,{upper-1},sequence,&steps)==RequestResult::GRANTED,"large request uses checked available/Need bounds");
    validateTrace(banker,true,sequence,steps);
}

void independentModels() {
    mt19937 generator(20261105);
    for (int trial=0;trial<500;trial++) {
        const int n=1+generator()%5,m=1+generator()%3;
        ResourceVector available(static_cast<size_t>(m)),total(static_cast<size_t>(m));
        ResourceMatrix allocation(static_cast<size_t>(n),ResourceVector(static_cast<size_t>(m))),maximum=allocation;
        for (int j=0;j<m;j++) { available[j]=generator()%4; total[j]=available[j]; }
        for (int i=0;i<n;i++) for (int j=0;j<m;j++) { allocation[i][j]=generator()%4; total[j]+=allocation[i][j]; }
        for (int i=0;i<n;i++) for (int j=0;j<m;j++) {
            maximum[i][j]=allocation[i][j]+generator()%static_cast<unsigned>(total[j]-allocation[i][j]+1);
        }
        Banker banker;
        string error;
        check(banker.initialize(available,maximum,allocation,error),"generated legal model initialized");
        const auto before=snapshot(banker);
        vector<int> sequence;
        vector<SafetyStep> steps;
        const bool safe=banker.checkSafety(sequence,&steps);
        check(safe==bruteSafe(available,maximum,allocation),"safety matches exhaustive permutation reference");
        validateTrace(banker,safe,sequence,steps);
        check(same(banker,before),"model safety read-only");

        const int pid=generator()%static_cast<unsigned>(n);
        ResourceVector request(static_cast<size_t>(m));
        bool exceeds=false,shortage=false;
        for (int j=0;j<m;j++) {
            request[j]=generator()%static_cast<unsigned>(maximum[pid][j]-allocation[pid][j]+3);
            exceeds=exceeds || request[j]>maximum[pid][j]-allocation[pid][j];
            shortage=shortage || request[j]>available[j];
        }
        auto trialAvailable=available;
        auto trialAllocation=allocation;
        RequestResult expected;
        if (exceeds) expected=RequestResult::EXCEEDS_NEED;
        else if (shortage) expected=RequestResult::INSUFFICIENT_AVAILABLE;
        else {
            for (int j=0;j<m;j++) { trialAvailable[j]-=request[j]; trialAllocation[pid][j]+=request[j]; }
            expected=bruteSafe(trialAvailable,maximum,trialAllocation) ? RequestResult::GRANTED : RequestResult::UNSAFE;
        }
        check(banker.requestResources(pid,request,sequence,&steps)==expected,"request matches independent exhaustive reference");
        if (expected==RequestResult::GRANTED) {
            check(banker.getAvailable()==trialAvailable && banker.getAllocation()==trialAllocation,"model approved state");
            validateTrace(banker,true,sequence,steps);
        } else check(same(banker,before) && sequence.empty(),"model rejection fully preserves original state");
    }
}

int main() {
    fixedCases();
    initializationGuards();
    requestCases();
    limits();
    independentModels();
    cout<<"Passed: "<<passed<<" Failed: "<<failed<<"; exhaustive model datasets: 500\n";
    return failed==0 ? 0 : 1;
}
