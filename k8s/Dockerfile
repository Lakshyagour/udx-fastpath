FROM ubuntu:24.04

RUN apt-get update 
RUN apt-get install -y \
    wget nano htop net-tools python3 net-tools iproute2 python3-pip python3-pyelftools tcpdump xz-utils sysstat pciutils build-essential 
RUN apt-get install -y \  
    libbsd-dev libelf-dev libjson-c-dev libnl-3-dev libnl-cli-3-dev libnuma-dev \
    libpcap-dev  libbpf-dev libxdp-dev libjsoncpp-dev
RUN apt-get install -y \
    pkg-config git gcc-multilib clang meson llvm lld m4 cmake 

RUN ln -s /usr/bin/python3 /usr/bin/python
RUN sed -i 's/ENABLED="false"/ENABLED="true"/' /etc/default/sysstat
RUN service sysstat restart

WORKDIR /home
RUN wget http://fast.dpdk.org/rel/dpdk-24.03.tar.xz
RUN tar -xJf dpdk-24.03.tar.xz dpdk-24.03/
RUN mv dpdk-24.03 dpdk
RUN rm -rf dpdk-24.03.tar.xz
WORKDIR /home/dpdk
RUN meson setup build
WORKDIR /home/dpdk/build
RUN ninja 
RUN meson install

WORKDIR /home
RUN git clone https://github.com/CloudNativeDataPlane/cndp.git
WORKDIR /home/cndp
RUN make
RUN make install
WORKDIR /home/cndp/usr/local
RUN cp -r lib/pkgconfig/* /usr/local/lib/x86_64-linux-gnu/pkgconfig/ 
RUN cp -r lib/x86_64-linux-gnu/* /usr/local/lib/x86_64-linux-gnu/
RUN cp -r include/* /usr/local/include/
RUN cp -r bin/* /usr/local/bin/

WORKDIR /home 
RUN rm -rf cndp 
RUN rm -rf dpdk
RUN apt-get install -y libboost-all-dev 

WORKDIR /home/ubuntu 
RUN touch hello1
RUN git clone https://github.com/Lakshyagour/udx-fastpath.git
WORKDIR /home/ubuntu/udx-fastpath
RUN bash build.sh




# docker save afxdp-device-plugin -o afxdp-device-plugin.tar
# sudo ctr -n=k8s.io images import afxdp-device-plugin.tar

