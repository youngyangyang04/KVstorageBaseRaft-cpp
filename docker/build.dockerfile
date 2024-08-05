FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get clean && \
    apt-get autoclean

RUN apt update && \ 
    apt install  -y \
    build-essential gdb vim htop cmake \
    apt-utils net-tools clang-format \
    wget bzip2 file python3-dev \
    python3-setuptools unzip zlib1g-dev


COPY install /tmp/install

RUN /tmp/install/build_boost.sh
# RUN /tmp/install/install_abseil.sh
RUN /tmp/install/build_protobuf.sh
RUN /tmp/install/build_muduo.sh

WORKDIR /work
 

