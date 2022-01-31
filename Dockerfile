# Dockerfile for installing WFDB
FROM debian-buster

COPY src /src/wfdb

WORKDIR  /src/wfdb

RUN COPY src .

# TODO after cmake integration: Insert install scripts. git clone, cmake
