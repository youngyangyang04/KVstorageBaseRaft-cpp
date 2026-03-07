//
// Created by swx on 23-12-23.
//

#ifndef CONFIG_H
#define CONFIG_H

const bool Debug = true;

const int debugMul = 1;  // 本地测试恢复为 1
const int HeartBeatTimeout = 25 * debugMul;  // 25ms
const int ApplyInterval = 10 * debugMul;     // 10ms

const int minRandomizedElectionTime = 300 * debugMul;  // 300ms
const int maxRandomizedElectionTime = 500 * debugMul;  // 500ms


const int CONSENSUS_TIMEOUT = 500 * debugMul;  // ms

// 协程相关设置

const int FIBER_THREAD_NUM = 1;              // 协程库中线程池大小
const bool FIBER_USE_CALLER_THREAD = false;  // 是否使用caller_thread执行调度任务

#endif  // CONFIG_H
