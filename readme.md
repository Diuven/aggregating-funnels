# Aggregating Funnels 

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

## How to run

1. Clone the repository

   - `git clone https://github.com/Diuven/aggregating-funnels.git && cd aggregating-funnels`

2. Build the docker image ([install docker](https://docs.docker.com/engine/install/) if you haven't)

   - `docker build --network=host --platform linux/amd64 -t aggfunnel .`

3. Launch the docker container as an interactive shell. Remaining commands should be run inside the docker container. 

   - `docker run -v .:/home/ubuntu/project -it --privileged --network=host aggfunnel`
   - This command mounts the aggregating-funnels directory into the docker container so changes to the aggregating-funnels director outside of the docker container will also affect the contents of the docker container and vice versa.

4. Run the benchmark. It is important to run the following commands from the default `/home/ubuntu/project` directory of the docker container.

   - Run scripts in `scripts/` directory
   - E.g. `python3 scripts/benchmarkRunner.py --task_path local/example.json`    [it will ask you to confirm the workload parameters. Press enter to confirm.]

5. Generate plots

   - Run scripts in `scripts/` directory, results to be saved in `results/` directory
