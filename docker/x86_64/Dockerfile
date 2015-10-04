FROM debian:jessie
MAINTAINER Victor Luchits <vluchits@gmail.com>
COPY qfusion-dev_0.3_all.deb /opt/
RUN apt-get update && apt-get install -y \
    gdebi-core \
    git \
    sudo && \
    gdebi -n /opt/qfusion-dev_0.3_all.deb
RUN groupadd -g 1066 buildbot && useradd -u 1066 -g 1066 -r -m buildbot
VOLUME /home/buildbot/tar_gz
USER buildbot
RUN cd /home/buildbot && \
    git clone https://github.com/Warsow/qfusion.git && \
    cd qfusion/source && \
    cmake -DQFUSION_TAR_GZ_OUTPUT_DIRECTORY=/home/buildbot/tar_gz .
