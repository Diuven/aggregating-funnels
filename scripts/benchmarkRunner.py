from collections import defaultdict
import os
import argparse
import subprocess
import datetime
import json

from benchmarkDigester import save_digest

thread_counts = {
    12: [1, 2, 4, 8, 12],
    32: [1, 2, 4, 8, 12, 16, 20, 24, 28, 32],
    36: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]
    + [17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36],
    160: [36, 48, 64, 80, 96, 112, 128, 144, 160],
    44: [1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44],
    64: [1, 2, 4, 8, 16, 32, 48, 64],
    20: [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20],
    50: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]
    + [18, 20, 22, 24, 26, 28, 30, 32, 36, 40, 44, 48, 50],
    96: [1, 2, 4, 8, 16, 32, 64, 96],
    128: [1, 2, 4, 8, 16, 32, 64, 96, 128],
    # 176: [16, 20, 24, 32, 36, 40, 48, 64, 80, 96, 112, 128, 144, 160, 176],
    176: [1, 4, 8, 12, 16, 24, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176],
    192: [1, 2, 4, 8, 16, 32, 64, 96, 128, 160, 192],
    224: [1, 2, 4, 8, 16, 32, 64, 96, 128, 160, 192, 224],
    360: [1, 2, 4, 8, 16, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 340, 360],
}

fixed_params_dict = {
    "counter": {
        "exec_path": "./build/counter_benchmark",
        "result_buffer_path": "./results_counter.csv",
        "result_save_path_prefix": "./results/counter/",
        # "args": ["0 100", "10 90", "50 50", "90 10"],  # rate of (read, add)
        "args": [
            "0 100 32",
            "10 90 32",
            "50 50 32",
            "90 10 32",
        ],  # rate of (read, add), and additional work
    },
}

model_choices = {
    "counter": [
        "stumpCounter",
        "rootStumpCounter",
        "fixedStumpCounter",
        "simpleAtomicCounter",
        "nestedStumpCounter",
        "combiningFunnelCounter",
        "emptyCounter",
    ],
}

default_malloc_path = "/usr/local/lib/libmimalloc.so"
default_reps = 3
default_milliseconds = 2000
interleave_exec_format = "LD_PRELOAD={malloc_path} numactl -i all {exec_path} {threads} {milliseconds} {params} 2> /dev/null"
single_exec_format = "LD_PRELOAD={malloc_path} numactl --cpunodebind=0 --membind=0 {exec_path} {threads} {milliseconds} {params} 2> /dev/null"
save_buffer_format = "{params}__{model_type}_{note}.csv"


def run_single_counter_model(
    model_type,
    run_threads,
    date_str,
    exec_format,
    malloc_path,
    milliseconds,
    reps,
    no_save,
    note,
):
    # Make clean and make counter benchmark
    subprocess.run(["make", "clean"])

    if "rootStumpCounter" in model_type:
        direct = model_type.split("_")[1]
        subprocess.run(
            ["make", "rootStumpCounter", f"DIRECT_COUNT={direct}"], check=True
        )
    elif "fixedStumpCounter" in model_type:
        fanout = model_type.split("_")[1]
        direct = model_type.split("_")[2]
        group = model_type.split("_")[3]
        subprocess.run(
            [
                "make",
                "fixedStumpCounter",
                f"FANOUT_COUNT={fanout}",
                f"DIRECT_COUNT={direct}",
                f"GROUP_SIZE={group}",
            ],
            check=True,
        )
    else:
        subprocess.run(["make", model_type], check=True)

    results = {}  # {detail_arg: {threads: avg_throughput, ...}, ...}

    exec_path = fixed_params_dict["counter"]["exec_path"]
    args_list = fixed_params_dict["counter"]["args"]
    result_buffer_path = fixed_params_dict["counter"]["result_buffer_path"]
    result_save_path_prefix = fixed_params_dict["counter"]["result_save_path_prefix"]
    result_save_path_prefix = os.path.join(result_save_path_prefix, date_str)

    for detail_arg in args_list:
        save_buffer_name = save_buffer_format.format(
            params=detail_arg.replace(" ", "_"),
            model_type=model_type,
            note=note,
        )
        save_buffer_path = os.path.join(
            result_save_path_prefix, "raw", save_buffer_name
        )
        os.makedirs(os.path.dirname(save_buffer_path), exist_ok=True)
        print(f"Running with params: {detail_arg}, will save at {save_buffer_path}")

        for r in range(reps):
            for threads in run_threads:
                cmd = exec_format.format(
                    malloc_path=malloc_path,
                    exec_path=exec_path,
                    threads=threads,
                    milliseconds=milliseconds,
                    params=detail_arg,
                )

                print(f"Rep {r+1} for {model_type}, run {cmd}")
                subprocess.run(cmd, shell=True, check=True)
                # relocate results_aux.csv
                aux_data_source_path = "./results_aux.csv"
                aux_data_target_name = f"aux_{threads}_{r+1}_{save_buffer_name}"
                aux_data_target_path = os.path.join(
                    result_save_path_prefix, "raw", aux_data_target_name
                )
                os.rename(aux_data_source_path, aux_data_target_path)
                print("\n")

        # Read the result buffer and save it to the file
        results[detail_arg] = {}
        with open(result_buffer_path, "r") as f:
            for line in f:
                tokens = line.split(",")
                _thread_count = int(tokens[0])
                _throughput = float(tokens[-1])
                if _thread_count not in results[detail_arg]:
                    results[detail_arg][_thread_count] = 0
                results[detail_arg][_thread_count] += _throughput / reps

        if not no_save:
            # move buffer to save
            os.rename(result_buffer_path, save_buffer_path)

        print(f"Done with params: {detail_arg}\n\n")

    return results


def main():
    task_path = "./local/task.json"
    task_info = json.load(open(task_path, "r"))
    datetime_str = datetime.datetime.now().strftime("%Y%m%dT%H")

    print(task_info["save_path"])
    save_path = task_info["save_path"].format(datetime_str=datetime_str)
    build_format = task_info["build_format"]
    exec_format = task_info["exec_format"]
    reps = task_info["repetition"]
    threads_list = task_info["threads_list"]
    trials = task_info["trials"]

    # make save_path
    os.makedirs(save_path, exist_ok=True)

    # Run
    print(f"Running benchmark for models: {task_path}, save at {save_path}")

    summary_results = {}
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
        subprocess.run(build_command, shell=True, check=True)

        for th in threads_list:
            for i in range(reps):
                exec_command = exec_format.format(
                    threads=th,
                    exec_params=exec_params,
                )
                print(f"Running {model_type} with {th} threads, rep {i+1}")
                subprocess.run(exec_command, shell=True, check=True)
                # read data(aux, main) and save it in df and tmp cache

        print(f"Done with {trial}")

    #     results = run_single_counter_model(
    #         model_type,
    #         threads_list,
    #         build_format,
    #         exec_format,
    #         reps,
    #         exec_params,
    #         save_path,
    #     )
    #     summary_results[model_type] = results

    # run_threads = (
    #     thread_counts[input_args.max_threads]
    #     if input_args.max_threads
    #     else input_args.threads
    # )
    # exec_format = (
    #     single_exec_format if input_args.single_socket else interleave_exec_format
    # )
    # if input_args.local:
    #     exec_format = "{exec_path} {threads} {milliseconds} {params} 2> /dev/null"

    # for model_type in input_args.model_types:
    #     result_buffer_path = fixed_params_dict["counter"]["result_buffer_path"]
    #     if os.path.exists(result_buffer_path):
    #         os.rename(
    #             result_buffer_path,
    #             result_buffer_path + f"_existed_{date_str}_{input_args.note}.csv",
    #         )
    #     model_results = run_single_counter_model(
    #         model_type,
    #         run_threads,
    #         date_str,
    #         exec_format,
    #         input_args.malloc,
    #         input_args.milliseconds,
    #         input_args.reps,
    #         input_args.no_save,
    #         input_args.note,
    #     )
    #     print(model_results)
    #     with open(f"results/counter/{date_str}/tmp_summary_{model_type}.csv", "w") as f:
    #         for detail_arg, detail_results in model_results.items():
    #             for thread, throughput in detail_results.items():
    #                 f.write(f"{detail_arg},{thread},{throughput}\n")
    #             f.write("\n")
    #     summary_results[model_type.replace("Counter", "")] = model_results

    # Save summary
    # if not input_args.no_digest:
    #     save_digest_path_prefix = os.path.join(
    #         fixed_params_dict["counter"]["result_save_path_prefix"],
    #         date_str,
    #         "digest",
    #     )
    #     save_digest(
    #         summary_results,
    #         save_digest_path_prefix,
    #         note=input_args.note,
    #     )

    print("Done with all models\n\n")


if __name__ == "__main__":
    main()
