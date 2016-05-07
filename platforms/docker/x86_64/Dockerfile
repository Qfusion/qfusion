FROM debian:jessie
MAINTAINER Victor Luchits <vluchits@gmail.com>
RUN groupadd -g 1066 buildbot && useradd -u 1066 -g 1066 -r -m buildbot
VOLUME /home/buildbot/tar_gz
ADD . /home/buildbot/qfusion
RUN apt-get update && apt-get install -y \
    gdebi-core \
    sudo && \
    gdebi -n /home/buildbot/qfusion/debian/*.deb
WORKDIR /home/buildbot/qfusion/source
RUN sudo chown -R buildbot:buildbot /home/buildbot/qfusion && \
    sudo -u buildbot cmake -DQFUSION_TAR_GZ_OUTPUT_DIRECTORY=/home/buildbot/tar_gz . && \
    sudo -u buildbot make clean
USER buildbot
