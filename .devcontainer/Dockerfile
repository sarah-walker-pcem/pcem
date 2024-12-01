FROM ubuntu:jammy

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends git libsdl2-dev libopenal-dev libwxgtk3.0-gtk3-dev \
    libpcap-dev cmake ninja-build build-essential ca-certificates
