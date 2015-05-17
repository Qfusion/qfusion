FROM 32bit/debian:jessie
MAINTAINER Victor Luchits <vluchits@gmail.com>
RUN apt-get update && apt-get install -y \
    cmake \
    git \
    mingw-w64 \
    sudo
RUN groupadd -g 1066 buildbot && useradd -u 1066 -g 1066 -r -m buildbot
VOLUME /home/buildbot/tar_gz
USER buildbot
RUN cd /home/buildbot && \
    git clone https://github.com/Warsow/qfusion.git && \
    cd qfusion/source && \
    cmake -DCMAKE_TOOLCHAIN_FILE=cmake/i686-mingw.cmake -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DQFUSION_TAR_GZ_OUTPUT_DIRECTORY=/home/buildbot/tar_gz .
