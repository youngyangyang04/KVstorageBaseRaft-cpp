## 1.通过项目中dockerfile文件，构建项目镜像 

```bash
cd KVstorageBaseRaft-cpp/docker
docker build --network host -t kv_raft:22.04  -f build.dockerfile .
```

## 2.进入docker容器

```bash
cd KVstorageBaseRaft-cpp/docker/scripts
#启动容器
./run_docker.sh
#进入容器
./into_docker.sh
```

## 3.在docker容器内编译代码

```bash
mkdir build
cd build
cmake ..
make -j4
```

## 4.容器内启动程序

```bash
cd ../bin
./raftCoreRun -n 3 -f test.conf
```

