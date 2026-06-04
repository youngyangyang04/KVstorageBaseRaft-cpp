# 使用 Ubuntu 22.04 作为基础镜像，它自带 GCC 11
FROM ubuntu:22.04

# 设置环境变量，避免交互式安装时的等待
ENV DEBIAN_FRONTEND=noninteractive

# 安装基础开发工具和依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    git \
    libboost-all-dev \
    libcurl4-openssl-dev \
    libjsoncpp-dev \
    libprotobuf-dev \
    protobuf-compiler \
    wget \
    vim \
    && rm -rf /var/lib/apt/lists/*

# 下载并安装 Muduo 库 (这是本项目最关键的依赖)
# 注意：Muduo 默认依赖 Boost
RUN git clone https://github.com/chenshuo/muduo.git /tmp/muduo \
    && cd /tmp/muduo \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local \
    && make -j$(nproc) \
    && make install \
    && cd / && rm -rf /tmp/muduo

# 设置工作目录
WORKDIR /workspace

# 保持容器运行
CMD ["/bin/bash"]
