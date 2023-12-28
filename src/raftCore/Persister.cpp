//
// Created by swx on 23-5-30.
//
#include "Persister.h"
#include "util.h"
//todo:如果文件出现问题会怎么办？？
void Persister::Save(const std::string raftstate, const std::string snapshot) {
    std::lock_guard<std::mutex> lg(mtx);
    // 将raftstate和snapshot写入本地文件
    std::ofstream outfile;
    m_raftStateOutStream<<raftstate;
    m_snapshotOutStream<<snapshot;
}

std::string Persister::ReadSnapshot() {
    std::lock_guard<std::mutex> lg(mtx);
    if(m_snapshotOutStream.is_open()){
        m_snapshotOutStream.close();
    }
    Defer ec1([this]()->void {
        this->m_snapshotOutStream.open(snapshotFile);
    });  //这个变量后生成，会先销毁
    std::fstream ifs(snapshotFile,std::ios_base::in);
    if(!ifs.good()){
        return "";
    }
    std::string  snapshot;
    ifs>>snapshot;
    ifs.close();
    return snapshot;
}

void Persister::SaveRaftState(const std::string& data) {
    std::lock_guard<std::mutex> lg(mtx);
    // 将raftstate和snapshot写入本地文件
    m_raftStateOutStream<<data;
}

long long  Persister::RaftStateSize() {
    std::lock_guard<std::mutex> lg(mtx);

    return m_raftStateSize;
}

std::string Persister::ReadRaftState() {
    std::lock_guard<std::mutex> lg(mtx);

    std::fstream ifs(raftStateFile,std::ios_base::in);
    if(!ifs.good()){
        return "";
    }
    std::string  snapshot;
    ifs>>snapshot;
    ifs.close();
    return snapshot;
}

Persister::Persister(int me) :raftStateFile("raftstatePersist"+ std::to_string(me)+".txt"),snapshotFile("snapshotPersist"+ std::to_string(me)+".txt") ,m_raftStateSize(0){

    std::fstream file(raftStateFile, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file.close();
    }
    file = std::fstream (snapshotFile, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file.close();
    }
    m_raftStateOutStream.open(raftStateFile);
    m_snapshotOutStream.open(snapshotFile);
}

Persister::~Persister() {
    if(m_raftStateOutStream.is_open()){
        m_raftStateOutStream.close();
    }
    if(m_snapshotOutStream.is_open()){
        m_snapshotOutStream.close();
    }

}



