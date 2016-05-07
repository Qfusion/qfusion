FROM 32bit/debian:jessie
MAINTAINER Victor Luchits <vluchits@gmail.com>
RUN groupadd -g 1066 buildbot && useradd -u 1066 -g 1066 -r -m buildbot
VOLUME /home/buildbot/tar_gz
ADD . /home/buildbot/qfusion
RUN apt-get update && apt-get install -y \
    cmake \
    mingw-w64 \
    sudo
WORKDIR /home/buildbot/qfusion/source
RUN sudo chown -R buildbot:buildbot /home/buildbot/qfusion && \
    sudo -u buildbot cmake -DCMAKE_TOOLCHAIN_FILE=cmake/i686-mingw.cmake -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DQFUSION_TAR_GZ_OUTPUT_DIRECTORY=/home/buildbot/tar_gz . && \
    sudo -u buildbot make clean
USER buildbot
