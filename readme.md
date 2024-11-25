
# Aggregating Funnels

Software artifact associated with the following paper:

Aggregating Funnels for Faster Fetch&Add and Queues\
Younghun Roh, Yuanhao Wei, Eric Ruppert, Panagiota Fatourou, Siddhartha Jayanti, Julian Shun \
ACM SIGPLAN Symposium on Principles and Practice of Parallel Programming (PPoPP 2025)

## Hardware/Software Requirements

- An Intel machine with 40+ cores (80+ hyperthreads) with Docker
- The full set of experiments takes ~10 hours to run

## Getting Started Guide

1. Clone the repository

   - `git clone https://github.com/Diuven/aggregating-funnels.git && cd aggregating-funnels`

2. Build the docker image ([install docker](https://docs.docker.com/engine/install/) if you haven't)

   - `docker build --network=host --platform linux/amd64 -t aggfunnel .`

3. Launch the docker container as an interactive shell. This command also complies all the necessary binaries. Remaining commands should be run inside the docker container. 

   - `docker run -v .:/home/ubuntu/project -it --privileged --network=host aggfunnel`
   - Note: This command mounts the current directory `aggregating-funnels/` into the docker container, so both are synchronized.

4. Testing
   - run `python3 scripts/benchmarkRunner.py --task_path local/example.json` inside the docker container
   - This should take a few minutes and produce an output graph in `results`

## Reproducing Paper Results 

5. Run the benchmark. 
   - Note: It is important to run the following commands from the default `/home/ubuntu/project` directory of the docker container.
   - Run scripts in `scripts/` directory
   - E.g. `python3 scripts/benchmarkRunner.py --task_path local/example.json`    [it will ask you to confirm the workload parameters. Press enter to confirm.]

6. Generate plots

   - Run scripts in `scripts/` directory, results to be saved in `results/` directory

## List of claims from the paper supported by the artifact

-   Given a machine with > 80 logical cores, the graphs generated should be similar to the ones reported in our paper (up to the corresponding core count)

## Directory Structure

- benchmark
  - with different models and settings
- profiling
- testing
- plotting

benchmark structure:

- cpp -> executable which runs single trial and gives results (in stdout)
- python to run sets of trials, and save the results, while giving some temporary prelim summaries
- run the python based on yml file?

raw names under date-time (up to mins)