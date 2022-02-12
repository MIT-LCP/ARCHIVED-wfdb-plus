# Dockerfile for installing WFDB
FROM debian-buster

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends libcurl4-nss-dev

COPY src /src/wfdb

WORKDIR  /src/wfdb

RUN COPY src .

# TODO after cmake integration: Insert install scripts. git clone, cmake
