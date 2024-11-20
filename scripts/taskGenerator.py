import json


def generate_save_path(note):
    res = "counter/{datetime_str}__"
    res += f"{note}/"
    return res


def generate_full_exec_format(malloc_path, exec_path, milliseconds):
    # "LD_PRELOAD={malloc_path} numactl -i all {exec_path} {threads} {milliseconds} {exec_params} 2> /dev/null"
    res = f"LD_PRELOAD={malloc_path} numactl -i all {exec_path} "
    res += "{threads} "
    res += f"{milliseconds} "
    res += "{exec_params} 2> /dev/null"
    return res


def generate_simple_exec_format(exec_path, milliseconds):
    # "{exec_path} {threads} {milliseconds} {exec_params} 2> /dev/null"
    res = f"{exec_path} "
    res += "{threads} "
    res += f"{milliseconds} "
    res += "{exec_params} 2> /dev/null"
    return res


def generate_threads_list(max_thread):
    if max_thread <= 16:
        res = [1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16]
        res = [x for x in res if x <= max_thread]
        return res
    elif max_thread <= 64:
        res = [1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64]
        res = [x for x in res if x <= max_thread]
        return res
    else:
        res = [1, 2, 4, 8, 12, 16, 24, 32, 40, 48, 56, 64]
        # append every 8
        for i in range(80, max_thread + 1, 16):
            res.append(i)
        return res


def get_exec_param(write_percent, additional_work):
    if not 0 <= write_percent <= 100:
        raise ValueError("write_percent should be between 0 and 100")
    return f"{100-write_percent} {write_percent} {additional_work}"


def get_single_trial(model_type, build_params, write_percent, additional_work):
    return {
        "model_type": model_type,
        "build_params": build_params,
        "exec_params": get_exec_param(write_percent, additional_work),
    }


model_type_list = [
    "hardwareCounter",
    "aggFunnelCounter",
    "recursiveAggFunnelCounter",
    "combFunnelCounter",
]
exec_params_list = [
    (90, 512),
    (100, 512),
    (50, 512),
    (10, 512),
    (90, 32),
]


def main():
    note = "test"
    full_exec = True
    malloc_path = "/usr/local/lib/libmimalloc.so"
    exec_path = "./build/counter_benchmark"
    max_threads = 196
    reps = 5
    ms = 2000
    task_path = "./local/task.json"

    task_info = {}
    task_info["save_path"] = generate_save_path(note)
    task_info["build_format"] = "make {model_type} {build_params}"
    if full_exec:
        task_info["exec_format"] = generate_full_exec_format(malloc_path, exec_path, ms)
    else:
        task_info["exec_format"] = generate_simple_exec_format(exec_path, 1000)
    task_info["repetition"] = reps
    task_info["threads_list"] = generate_threads_list(max_threads)

    task_info["trials"] = []
    for model_type in model_type_list:
        for exec_params in exec_params_list:
            task_info["trials"].append(
                get_single_trial(model_type, "", exec_params[0], exec_params[1])
            )

    total_exec = reps * len(task_info["threads_list"]) * len(task_info["trials"])
    total_min = total_exec * ms / 1000 / 60
    print(json.dumps(task_info, indent=2))
    print(f"Estimated total execution time: {total_min} minutes")
    print(f"Writing to {task_path}")

    with open(task_path, "w") as f:
        json.dump(task_info, f, indent=2)


if __name__ == "__main__":
    main()
