EDF（Earliest Deadline First，最早截止时间优先）调度算法是一种经典的实时调度策略，核心思想是 任务的优先级由其截止时间动态决定 ，截止时间越早的任务优先级越高。以下从原理、实现和内核应用三个维度详细说明：

### 一、算法核心原理
1. 优先级动态分配
   
   - 每个任务关联一个 绝对截止时间 （任务必须完成的最终时间点）
   - 调度器始终选择当前系统中截止时间最早的任务执行
   - 新任务到达时，若其截止时间早于当前运行任务，则触发抢占
2. 关键特性
   
   - 可调度性判定 ：若任务集总利用率 ≤ 100%，则 EDF 可保证所有任务 deadlines 满足（Liu & Layland 定理）
   - 动态性 ：优先级随任务截止时间动态变化，而非静态分配
   - 抢占式 ：支持任务间抢占，确保紧急任务优先执行

```
// EDF 就绪队列结构
typedef struct {
    SortLinkAttribute sortLink;       // 按截止时间排序的链表（依赖 <mcfile name="los_sortlink.c" path="d:/project/kernel_liteos_a_note/kernel/base/sched/los_sortlink.c"></mcfile>）
    UINT32 taskCount;                 // 任务数量
    UINT64 latestDeadline;            // 最近截止时间
} EDFRunqueue;
```
```
const STATIC SchedOps g_deadlineOps = {
    .enqueue = EDFEnqueue,            // 按截止时间插入任务（核心排序逻辑）
    .schedParamCompare = EDFParamCompare, // 比较任务截止时间（决定优先级）
    .priorityInheritance = EDFPriorityInheritance, // 解决优先级反转
    .timeSliceUpdate = EDFTimeSliceUpdate, // 更新任务剩余执行时间
    // ... 其他调度生命周期函数
};
```