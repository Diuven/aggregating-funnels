# generic ubuntu image with some utils
FROM ubuntu:24.04

# Install some basic utilities
RUN apt-get update && apt-get install -y \
    curl wget vim git htop \
    cmake g++ numactl python3 zsh \
    python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*
# oh-my-zsh
RUN sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

# Install mimalloc
RUN git clone https://github.com/microsoft/mimalloc.git
RUN cd mimalloc
RUN mkdir -p out/release
RUN cd out/release

RUN cmake ../..
RUN make
RUN sudo make install
RUN cd

# Clone the repo
RUN git clone https://github.com/Diuven/aggregating-funnels.git
RUN cd aggregating-funnels

# entry point
CMD ["/bin/zsh"]