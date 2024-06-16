FROM ubuntu:24.04

RUN apt-get update 
RUN apt-get install -y \
    wget nano htop net-tools python3 net-tools iproute2 python3-pip  tcpdump xz-utils sysstat pciutils build-essential 
RUN apt-get install -y \  
    libbsd-dev libelf-dev libjson-c-dev libnl-3-dev libnl-cli-3-dev libnuma-dev \
    libpcap-dev  libbpf-dev libxdp-dev
RUN apt-get install -y \
    pkg-config git gcc-multilib clang meson llvm lld m4 

RUN ln -s /usr/bin/python3 /usr/bin/python
RUN apt-get install python3-pyelftools
RUN sed -i 's/ENABLED="false"/ENABLED="true"/' /etc/default/sysstat
RUN service sysstat restart


WORKDIR /home
RUN wget https://fast.dpdk.org/rel/dpdk-22.11.5.tar.xz
RUN tar -xJf dpdk-22.11.5.tar.xz dpdk-stable-22.11.5/
WORKDIR /home/dpdk-stable-22.11.5/
RUN meson setup build
WORKDIR /home/dpdk-stable-22.11.5/build
RUN ninja 
RUN meson install

WORKDIR /home
RUN git clone https://github.com/CloudNativeDataPlane/cndp.git
WORKDIR /home/cndp
RUN make
RUN make install



