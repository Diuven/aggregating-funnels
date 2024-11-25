import os
import subprocess
import datetime
import json
import pandas as pd
import argparse
from pprint import pprint


def main():
    if not os.path.exists("results"):
        os.makedirs("results")
                    
    # get task_path from argparse. if none given, use default
    # task
    parser = argparse.ArgumentParser(description="Run benchmark")
    parser.add_argument(
        "--task_path",
        type=str,
        default="./local/task.json",
        help="Path to the task file",
    )
    args = parser.parse_args()

    task_path = args.task_path
    task_info = json.load(open(task_path, "r"))
    datetime_str = datetime.datetime.now().strftime("%Y%m%dT%H")

    save_path = task_info["save_path"].format(datetime_str=datetime_str)
    build_format = task_info["build_format"]
    exec_format = task_info["exec_format"]
    reps = task_info["repetition"]
    threads_list = task_info["threads_list"]
    trials = task_info["trials"]

    print("Running benchmark with the following parameters:")
    pprint(
        {
            "save_path": save_path,
            "build_format": build_format,
            "exec_format": exec_format,
            "reps": reps,
            "threads_list": threads_list,
            "trial_example": trials[0],
        },
    )
    input("Press Enter to continue...")

    # make save_path
    os.makedirs(save_path, exist_ok=True)

    # Run
    print(f"Running benchmark for models: {task_path}, save at {save_path}")

    main_df = pd.DataFrame()
    aux_df = pd.DataFrame()
    total_exec = reps * len(threads_list) * len(trials)
    ms = int(task_info["exec_format"].split(" ")[-4])
    total_min = total_exec * ms / 1000 / 60
    print(f"Estimated total execution time: {total_min} minutes for {total_exec} runs")

    for trial in trials:
        model_type = trial["model_type"]
        build_params = trial["build_params"]
        exec_params = trial["exec_params"]

        build_command = build_format.format(
            model_type=model_type, build_params=build_params
        )
        print(f"Building {model_type} with {build_params}")
        subprocess.run(build_command.strip().split(" "), check=True)

        for th in threads_list:
            for i in range(reps):
                exec_command = exec_format.format(
                    threads=th,
                    exec_params=exec_params,
                )
                print(f"Running {model_type} with {th} threads, rep {i+1}")
                subprocess.run(exec_command, shell=True, check=True)
                # read data and concat
                cur_main_df = pd.read_csv("results/counter_main.csv")
                cur_main_df["model"] = model_type
                cur_main_df["build_params"] = build_params
                cur_main_df["exec_params"] = exec_params
                cur_main_df["threads"] = th
                cur_main_df["rep"] = i
                cols = cur_main_df.columns.tolist()
                cols = cols[-5:] + cols[:-5]
                cur_main_df = cur_main_df[cols]

                cur_aux_df = pd.read_csv("results/counter_aux.csv")
                cur_aux_df["model"] = model_type
                cur_aux_df["build_params"] = build_params
                cur_aux_df["exec_params"] = exec_params
                cur_aux_df["threads"] = th
                cur_aux_df["rep"] = i
                cols = cur_aux_df.columns.tolist()
                cols = cols[-5:] + cols[:-5]
                cur_aux_df = cur_aux_df[cols]

                main_df = pd.concat([main_df, cur_main_df])
                aux_df = pd.concat([aux_df, cur_aux_df])

        main_df.to_csv(os.path.join(save_path, "main.csv"), index=False)
        aux_df.to_csv(os.path.join(save_path, "aux.csv"), index=False)
        print(f"Done with {trial}")

    print("Done with all models\n\n")


if __name__ == "__main__":
    main()
