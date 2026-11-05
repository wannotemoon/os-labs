#ifndef OS_BANKER_H
#define OS_BANKER_H

#include <string>
#include <vector>

using ResourceVector=std::vector<long long>;
using ResourceMatrix=std::vector<ResourceVector>;

struct SafetyStep {
    int processId;
    ResourceVector workBefore;
    ResourceVector released;
    ResourceVector workAfter;
};

enum class RequestResult {
    GRANTED,
    INVALID_REQUEST,
    EXCEEDS_NEED,
    INSUFFICIENT_AVAILABLE,
    UNSAFE
};

// 独立资源分配模型；P0...Pn-1是此案例的编号，不复用CPU调度的PCB或分页位示图。
class Banker {
private:
    ResourceVector available;
    ResourceMatrix maximum;
    ResourceMatrix allocation;

public:
    // 校验维度、非负数、Allocation<=Max、总量不溢出及Max不超过系统总量。
    // 数据合法但状态不安全仍允许载入，便于观察不安全案例；失败保留原数据。
    bool initialize(const ResourceVector& available,const ResourceMatrix& maximum,
                    const ResourceMatrix& allocation,std::string& error);

    int processCount() const;
    int resourceCount() const;
    const ResourceVector& getAvailable() const;
    const ResourceMatrix& getMaximum() const;
    const ResourceMatrix& getAllocation() const;
    ResourceMatrix getNeed() const; // Need=Max-Allocation，现场计算，用户不用另输入一遍。

    // 只在Work/Finish副本上假想完成，绝不真的回收资源。
    // true时sequence是完整安全序列；false时只是已能完成的前缀，不能称为安全序列。
    bool checkSafety(std::vector<int>& sequence,std::vector<SafetyStep>* steps=nullptr) const;

    // 三步：检查需求/可用量，试分配，安全性检测；只有安全时才正式保存。
    // 任意拒绝都保持全部资源表不变；GRANTED时返回试分配后的完整安全序列。
    // steps可选，UNSAFE时也可查看试分配的推演前缀，实际状态仍不变。
    RequestResult requestResources(int processId,const ResourceVector& request,std::vector<int>& sequence,
                                   std::vector<SafetyStep>* steps=nullptr);
};

#endif // OS_BANKER_H
