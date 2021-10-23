FROM ubuntu:18.04

# Update packages and setup timezone
RUN apt-get update && apt-get -y upgrade && \
      apt-get -y install tzdata

ENV TZ=Europe/Zurich
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && \
      echo $TZ > /etc/timezone
RUN dpkg-reconfigure --frontend=noninteractive tzdata


RUN apt-get -y install file unzip zip xz-utils git \
                         gcc g++ cmake default-jdk \
                         python3

RUN apt-get -y install tmux sudo iproute2

ARG user=app
ARG group=app
ARG uid=1000
ARG gid=1000

RUN addgroup --gid ${gid} ${group} && \
        adduser --uid ${uid} \
                --ingroup ${group} \
                --gecos "" \
                --disabled-password \
                --shell /bin/bash \
                ${user}

RUN adduser app sudo && \
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers && \
    mkdir -p /app && \
    chown -R ${user}:${user} /app

USER ${user}

WORKDIR /app
