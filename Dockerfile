# generic ubuntu image with some utils
FROM ubuntu:22.04

# Install some basic utilities
RUN apt-get update && apt-get install -y \
    curl wget vim git htop sudo \
    cmake g++ numactl python3 zsh \
    python3 python3-pip python3-venv \
    clang-13 libc++abi1-13 libc++abi-13-dev libc++-13-dev pkg-config autoconf \
    && rm -rf /var/lib/apt/lists/*
# oh-my-zsh
RUN sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

# Install mimalloc
RUN git clone https://github.com/microsoft/mimalloc.git /usr/mimalloc
RUN mkdir -p /usr/mimalloc
WORKDIR /usr/mimalloc
RUN mkdir -p out/release
WORKDIR /usr/mimalloc/out/release

RUN cmake ../..
RUN make
RUN make install


# Install jemalloc
WORKDIR /usr/
RUN curl -L -o jemalloc.tar.gz https://github.com/jemalloc/jemalloc/archive/refs/tags/5.3.0.tar.gz
RUN tar -xzf jemalloc.tar.gz && rm jemalloc.tar.gz
WORKDIR /usr/jemalloc-5.3.0
RUN ls -al
RUN ./autogen.sh && ./configure
RUN make -j16
RUN make install
WORKDIR /
RUN rm -rf /usr/jemalloc-5.3.0


WORKDIR /home/ubuntu/
RUN apt-get update && DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt-get install -y tzdata
RUN apt-get update && apt-get install -y python3-matplotlib python3-numpy python3-pandas python3-plotly

# Create a mount point for the repository
VOLUME /home/ubuntu/project

# entry point
WORKDIR /home/ubuntu/project
ENTRYPOINT ["/bin/zsh"]