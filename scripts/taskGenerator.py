import json
import os


def generate_save_path(note):
    res = "./results/counter/{datetime_str}__"
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


def generate_exec_format(exec_path, milliseconds, use_numa, malloc_path):
    # "LD_PRELOAD={malloc_path} numactl -i all {exec_path} {threads} {milliseconds} {exec_params} 2> /dev/null"
    res = ""
    if malloc_path is not None and malloc_path != "":
        res = f"LD_PRELOAD={malloc_path} "
    if use_numa:
        res += "numactl -i all "

    res += f"{exec_path} "
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
    # additional work=32 -> 32 iterations -> 512 additaional cycles
    if not 0 <= write_percent <= 100:
        raise ValueError("write_percent should be between 0 and 100")
    return f"{100-write_percent} {write_percent} {additional_work}"


def get_single_trial(model_type, build_params, write_percent, additional_work):
    return {
        "model_type": model_type,
        "build_params": build_params,
        "exec_params": get_exec_param(write_percent, additional_work),
    }


def prompt(default, prompt_str):
    res = input(f"{prompt_str} (default: {default}): ")
    if res == "":
        return default
    return res


def ask_for_input():
    is_numa = os.system("numactl --show > /dev/null 2>&1") == 0
    print(f"NUMA availability: {is_numa}")

    def_malloc_path = "/usr/local/lib/libmimalloc.so"
    # check if malloc default path is valid
    if not os.path.exists(def_malloc_path):
        print(f"Default malloc path {def_malloc_path} does not exist")
        def_malloc_path = ""
    else:
        print(f"MiMalloc detected: {def_malloc_path}")

    def_max_threads = str(os.cpu_count())
    print(f"Available threads: {def_max_threads}")

    use_numa = prompt("yes" if is_numa else "no", "Use NUMA").lower() in ["y", "yes"]
    use_malloc = prompt(
        "yes" if def_malloc_path != "" else "no", "Use custom malloc"
    ).lower() in ["y", "yes"]
    if not use_malloc:
        malloc_path = ""
    else:
        malloc_path = prompt(def_malloc_path, f"Malloc path")
    exec_path = prompt("./build/counter_benchmark", "Executable path")
    max_threads = int(prompt(def_max_threads, f"Max threads"))
    reps = int(prompt("5", "Repetitions"))
    ms = int(prompt("2000", "Milliseconds"))
    preset = prompt("figure4", "Preset")
    task_path = prompt("./local/task.json", "Resulting task path")
    note = prompt(preset, "Note")

    # check if malloc path is valid
    if malloc_path != "" and not os.path.exists(malloc_path):
        raise ValueError(f"Invalid malloc path {malloc_path}")

    return (
        use_numa,
        malloc_path,
        exec_path,
        max_threads,
        reps,
        ms,
        preset,
        task_path,
        note,
    )


def trials_preset_figure3():
    res_list = []
    # confRootAggFunnelCounter
    build_params = "DIRECT_COUNT=0 AUX_DATA=1"
    res_list.append(get_single_trial("confRootAggFunnelCounter", build_params, 90, 32))
    res_list.append(get_single_trial("confRootAggFunnelCounter", build_params, 50, 32))

    # configuredAggFunnelCounter
    for agg_count in [4, 6, 8, 10]:
        build_params = f"AGG_COUNT={agg_count} DIRECT_COUNT=0 AUX_DATA=1"
        res_list.append(
            get_single_trial("configuredAggFunnelCounter", build_params, 90, 32)
        )
        res_list.append(
            get_single_trial("configuredAggFunnelCounter", build_params, 50, 32)
        )

    # hardwareCounter
    res_list.append(get_single_trial("hardwareCounter", "AUX_DATA=1", 90, 32))
    res_list.append(get_single_trial("hardwareCounter", "AUX_DATA=1", 50, 32))

    return res_list


def trials_preset_figure4():
    models_fig4 = [
        "aggFunnelCounter",
        "combFunnelCounter",
        "recursiveAggFunnelCounter",
        "hardwareCounter",
    ]
    exec_params_fig4 = [
        (90, 32),
        (100, 32),
        (50, 32),
        (10, 32),
        (90, 4),
    ]

    res_list = []
    for model_type in models_fig4:
        for exec_params in exec_params_fig4:
            res_list.append(
                get_single_trial(model_type, "", exec_params[0], exec_params[1])
            )
    return res_list


def trials_preset_figure5():
    res_list = []
    # configured aggFunnelCounter
    for agg_count in [2, 6]:
        for direct_count in [0, 1, 2]:
            build_params = (
                f"AGG_COUNT={agg_count} DIRECT_COUNT={direct_count} AUX_DATA=1"
            )
            res_list.append(
                get_single_trial("configuredAggFunnelCounter", build_params, 90, 4)
            )

    # hardwareCounter
    res_list.append(get_single_trial("hardwareCounter", "AUX_DATA=1", 90, 4))
    return res_list


def main():
    use_numa, malloc_path, exec_path, max_threads, reps, ms, preset, task_path, note = (
        ask_for_input()
    )

    task_info = {}
    task_info["save_path"] = generate_save_path(note)
    task_info["build_format"] = "make {model_type} {build_params}"
    task_info["exec_format"] = generate_exec_format(
        exec_path, ms, use_numa, malloc_path
    )
    task_info["repetition"] = reps
    task_info["threads_list"] = generate_threads_list(max_threads)

    task_info["trials"] = []
    if preset == "figure3":
        task_info["trials"] = trials_preset_figure3()
    elif preset == "figure4":
        task_info["trials"] = trials_preset_figure4()
    elif preset == "figure5":
        task_info["trials"] = trials_preset_figure5()
    else:
        raise ValueError(f"Unknown preset {preset}")

    total_exec = reps * len(task_info["threads_list"]) * len(task_info["trials"])
    total_min = total_exec * ms / 1000 / 60
    print(json.dumps(task_info, indent=2))
    print(f"Estimated total execution time: {total_min} minutes")
    print(f"Writing to {task_path}")

    with open(task_path, "w") as f:
        json.dump(task_info, f, indent=2)


if __name__ == "__main__":
    main()
