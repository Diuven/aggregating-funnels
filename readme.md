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
   - Run `python3 scripts/benchmarkRunner.py --task_path local/example.json` inside the docker container
   - This should take a few minutes and produce an output graph in `results`

## Reproducing Paper Results

5. Run the benchmark.

   - Running `./scripts/run_figures.sh` will run all the experiments and generate the figures in the `results/plots` directory. This will take a couple of hours to run, depending on the number of threads available. stdout will show estimated time to completion for each figure.
   - During the benchmark, you can monitor the progress by checking stdout and the contents of the `results/` directory. `log.txt` will have the output logs, and `main.csv` will have the raw data for each run.

## Customizing the Benchmark

- The default workflow, as specified in `scripts/run_figures.sh`, is to run `taskGenerator` to generate task specs, run `benchmarkRunner` to run the tasks with the specified parameters, and finally run `plotDrawer` to generate the plots from the results.
  - `taskGenerator` generates the json file and saves to `local` directory. You can directly inspect and edit the json file (recommended), or run `python3 scripts/taskGenerator.py` to walk through the prompts and generate a custom task spec.
  - `benchmarkRunner` runs the tasks specified in the given json file. You can change the json file with `--task_path` option. It saves the results in the `results` directory, in a subdirectory specified by the json file's `save_path` field.
  - `plotDrawer` generates the plots from the results. It currently supports generating the plots for the figures in the paper. You can change `--data_path`, `--save_path`, and `--figure_num` options.

## List of claims from the paper supported by the artifact

- Given a machine with > 80 logical cores, the graphs generated should be similar to the ones reported in our paper (up to the corresponding core count)
