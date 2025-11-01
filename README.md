# KVstorageBaseRaft-cpp

本项目目前只在[知识星球](https://programmercarl.com/other/kstar.html)答疑并维护。

[代码随想录知识星球](https://programmercarl.com/other/kstar.html)分布式存储项目目前已经做了全面升级：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20241209105519.png' width=500 alt=''></img></div>

相对于第一版补充如下内容：

学习这个项目的前提知识：

* 学习这个项目需要掌握的技能部分，做了更详细的说明。比如c++基础要求、raft的学习博客、KV的学习博客、RPC的博客提供学习。
* 补充了学习raft或rpc的连接、视频等，以及RPC在本项目中运用的测试文件提供学习。

代码模块讲解补充：

* 补充了部分注释，简化了原文档冗余的注释。
* 增加了部分关键函数的流程解释。
* 做了更清晰的目录。
* 补充了RPC部分在原文档中的仓库位置，实现了方便阅读方便查看。
* 对原文的内容进行整合基本原文档的部分内容有关代码部分全在该模块，并且通过目录就可以学习你想要的。

## 项目背景相关

### 背景

在当今大规模分布式系统的背景下，需要可靠、高可用性的分布式数据存储系统。

传统的集中式数据库在面对大规模数据和高并发访问时可能面临单点故障和性能瓶颈的问题。

为了解决这些问题，本项目致力于构建一种基于Raft一致性算法的分布式键值存储数据库，以确保数据的一致性、可用性和分区容错性。

### 目的

学习了Raft算法之后手动实现，**并基于此搭建了一个k-v存储的分布式数据库**。

### 解决的问题

- **一致性：** 通过Raft算法确保数据的强一致性，使得系统在正常和异常情况下都能够提供一致的数据视图。
- **可用性：** 通过分布式节点的复制和自动故障转移，实现高可用性，即使在部分节点故障的情况下，系统依然能够提供服务。
- **分区容错：** 处理网络分区的情况，确保系统在分区恢复后能够自动合并数据一致性。

### 技术栈

- **Raft一致性算法：** 作为核心算法，确保数据的一致性和容错性。
- **存储引擎：** 使用适当的存储引擎作为底层存储引擎，提供高效的键值对操作。目前选择的是跳表，但是可以替换为任意k-v数据库。

### 项目范围

项目的初始版本将实现基本的Raft协议和键值存储功能。

后续版本可能包括性能优化、安全性增强、监控和管理工具的开发等。

## 前置知识储备

在学习该项目之前，必须知道的内容有：

* 语言基础，比如：`mutex` ，什么是序列化和反序列化
* RPC相关，至少要知道什么是RPC

最好知道的内容有：

- c11的部分新特性：`auto` 、`RAII`等
- 分布式的基础概念：容错、复制等

## 你的收获

- Raft共识算法的快速理解
- 基于共识算法怎么搭建一个分布式的k-v数据库

需要注意的是，分布式式的共识算法实现本身是一个比较严谨的过程。

因为其本身的存在是为了多个服务器之间通过共识算法达成一致性的状态，从而避免单个节点不可用而导致整个集群不可用。

因此在学习过程中必须要考虑不同情况下节点宕机、断网情况下的影响。

许多情况需要仔细思考并实验以验证算法正确性，其中的思考别人无法代替，本项目的内容**只能作为分布式共识算法Raft的一个入门的实现，方便大家快速理解Raft算法**，从而写到简历上，如果想全部理解分布式算法的精髓只能多思考多看多总结。

mit6.824课程，如果你已经学习过该课程，那么已经不需要本项目了，本项目的难度和内容小于该课程。

## 最佳食用指南

**关注Raft算法本身**：首先整个项目最重点也是最难点的地方就是Raft算法本身的理解与实现，其他的部分都是辅助，因此在学习的过程中也最好关注Raft算法本身的实现与Raft类对外暴露的一些接口。

**多思考错误情况下的算法正确性**：Raft算法本身并不难理解，代码也并不多，但是简单的代码如何保证在复杂情况下的容错呢？需要在完成代码后多思考在代码不同运行阶段如果发生宕机等错误时的正确性。

## 项目大纲

项目的大概框图如下：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20240314115639.png' width=500 alt=''></img></div>

项目大概可以分为以下几个部分：

- **raft节点**：raft算法实现的核心层，负责与其他机器的raft节点沟通，达到 分布式共识 的目的。
- **raftServer**：负责raft节点与k-v数据库中间的协调服务；负责持久化k-v数据库的数据（可选）。
- **上层状态机**（k-v数据库）：负责数据存储。
- **持久层**：负责相关数据的落盘，对于raft节点，根据共识算法要求，必须对一些关键数据进行落盘处理，以保证节点宕机后重启程序可以恢复关键数据；对于raftServer，可能会有一些k-v数据库的东西需要落盘持久化。
- **RPC通信**：在 领导者选举、日志复制、数据查询、心跳等多个Raft重要过程中提供多节点快速简单的通信能力。

> 目前规划中没有实现节点变更功能或对数据库的切片等更进阶的功能，后面考虑学习加入。

在多个机器启动后，各个机器之间通过网络通信，构建成一个集群，对这样的集群，其对外表现的就像一台单机的k-v数据库一样，且少数节点出现故障不会影响整个集群的工作。

因此有了Raft算法的集群k-v数据库相对于单机的k-v数据库：

优势：集群有了容错的能力，可以理解成Raft算法可以保证各个机器上的k-v数据库（也称状态机）以相同的顺序执行外部命令。

劣势：容错能力需要算法提供，因此程序会变得复杂；需要额外对数据进行备份；需要额外的网络通信开销。

也是因此，其实上层的k-v数据库可以替换成其他的组件，毕竟只是一个状态机而已。

目前设计的后续主要内容：

1.`Raft`算法的一些概念性内容，比如：Raft算法是什么？Raft算法怎么完成公式？完成Raft算法需要哪几个主要函数？需要哪几个主要的变量维护？

2.`Raft`算法的主要函数实现思路及代码，主要函数包括：`AppendEntries` `sendRequestVote` `sendAppendEntries` `RequestVote` 等

3.其他部分组件，包括：RPC通信组件、k-v数据库、中间沟通数据库和raft节点的`raftServer`

## 项目难点

难点就是项目主要的几个功能模块的实现。

- Raft算法的理解与实现
- RPC通信框架的理解与实现
- k-v数据库

## 简历写法

学习完本项目，如何写到简历上呢？

在知识星球专栏里会给出本项目的简历写法，为了不让 这些写法重复率太高，所以公众号上是打码的。

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20241209105923.png' width=500 alt=''></img></div>

## 本项目常见问题

同时项目专栏也会针对本项目的常见问题，经行归类总结，并持续更新

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20241209105858.png' width=500 alt=''></img></div>

## 项目优化点

大家做这个项目，完成基础功能之后，可以按照如下方向继续优化这个项目：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20241215174241.png' width=500 alt=''></img></div>

## 项目专栏部分截图

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20241209105756.png' width=500 alt=''></img></div>

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20241209105815.png' width=500 alt=''></img></div>

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20241209105829.png' width=500 alt=''></img></div>

-------------------

## 分布式存储项目专栏

**本文档仅为星球内部专享，大家可以加入[知识星球](https://programmercarl.com/other/kstar.html)里获取，在星球置顶一**

## Star History

<a href="https://star-history.com/#youngyangyang04/KVstorageBaseRaft-cpp&Date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=youngyangyang04/KVstorageBaseRaft-cpp&type=Date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=youngyangyang04/KVstorageBaseRaft-cpp&type=Date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=youngyangyang04/KVstorageBaseRaft-cpp&type=Date" />
  </picture>
</a>


