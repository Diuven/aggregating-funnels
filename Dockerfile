# generic ubuntu image with some utils
FROM ubuntu:24.04

# Install some basic utilities
RUN apt-get update && apt-get install -y \
    curl wget vim git htop sudo \
    cmake g++ numactl python3 zsh \
    python3 python3-pip python3-venv \
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

WORKDIR /home/ubuntu/
RUN /bin/zsh -c "pip install --break-system-packages matplotlib numpy pandas plotly jupyter"

# Create a mount point for the repository
VOLUME /home/ubuntu/project

# entry point
WORKDIR /home/ubuntu/project
ENTRYPOINT ["/bin/zsh"]