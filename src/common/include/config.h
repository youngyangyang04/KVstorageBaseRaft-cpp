//
// Created by swx on 23-12-23.
//

#ifndef CONFIG_H
#define CONFIG_H

const bool Debug = true;

const int debugMul = 5;  // 时间单位系数，调大以适应公网延迟和慢速 CPU
const int HeartBeatTimeout = 25 * debugMul;  // 125ms
const int ApplyInterval = 10 * debugMul;     // 50ms

const int minRandomizedElectionTime = 500 * debugMul;  // 2.5s
const int maxRandomizedElectionTime = 1000 * debugMul; // 5.0s


const int CONSENSUS_TIMEOUT = 500 * debugMul;  // ms

// 协程相关设置

const int FIBER_THREAD_NUM = 1;              // 协程库中线程池大小
const bool FIBER_USE_CALLER_THREAD = false;  // 是否使用caller_thread执行调度任务

#endif  // CONFIG_H
