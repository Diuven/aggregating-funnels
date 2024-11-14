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

3. Run the docker container

   - `docker run -v .:/home/ubuntu/project -it --privileged aggfunnel`

4. Run the benchmark

   - Run scripts in `scripts/` directory

5. Generate plots

   - Run scripts in `scripts/` directory, results to be saved in `results/` directory
