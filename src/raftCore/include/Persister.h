//
// Created by swx on 23-5-30.
//

#ifndef SKIP_LIST_ON_RAFT_PERSISTER_H
#define SKIP_LIST_ON_RAFT_PERSISTER_H
#include <mutex>
#include <fstream>
class Persister{
private:
    std::mutex mtx;
    std::string m_raftState;
    std::string m_snapshot;
    const std::string raftStateFile;
    const std::string snapshotFile;
    std::ofstream m_raftStateOutStream;
    std::ofstream m_snapshotOutStream;
    long long m_raftStateSize; //避免每次都读取文件的具体大小
public:
    void Save(std::string raftstate , std::string snapshot );
    std::string ReadSnapshot();
    void SaveRaftState(const std::string& data);
    long long RaftStateSize();
    std::string ReadRaftState();
    explicit Persister(int me);
    ~Persister();
};


#endif //SKIP_LIST_ON_RAFT_PERSISTER_H
