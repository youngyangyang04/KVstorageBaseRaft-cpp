//
// Created by swx on 23-5-30.
//

#ifndef SKIP_LIST_ON_RAFT_PERSISTER_H
#define SKIP_LIST_ON_RAFT_PERSISTER_H
#include <fstream>
#include <mutex>
class Persister {
 private:
  std::mutex m_mtx;
  std::string m_raftState;
  std::string m_snapshot;
  /**
   * m_raftStateFileName: raftState文件名
   */
  const std::string m_raftStateFileName;
  /**
   * m_snapshotFileName: snapshot文件名
   */
  const std::string m_snapshotFileName;
  /**
   * 保存raftState的输出流
   */
  std::ofstream m_raftStateOutStream;
  /**
   * 保存snapshot的输出流
   */
  std::ofstream m_snapshotOutStream;
  /**
   * 保存raftStateSize的大小
   * 避免每次都读取文件来获取具体的大小
   */
  long long m_raftStateSize;

 public:
  void Save(std::string raftstate, std::string snapshot);
  std::string ReadSnapshot();
  void SaveRaftState(const std::string& data);
  long long RaftStateSize();
  std::string ReadRaftState();
  explicit Persister(int me);
  ~Persister();

 private:
  void clearRaftState();
  void clearSnapshot();
  void clearRaftStateAndSnapshot();
};

#endif  // SKIP_LIST_ON_RAFT_PERSISTER_H
