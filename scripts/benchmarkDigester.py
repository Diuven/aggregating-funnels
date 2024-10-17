from collections import defaultdict
import os
import argparse
from statistics import stdev
import matplotlib.pyplot as plt


save_summary_format = "summary_{params}__{note}.csv"
save_plotdata_format = "plotdata_{params}__{note}.csv"


def get_avg(lst):
    return sum(lst) / len(lst) if len(lst) > 0 else 0


def get_stddev(lst):
    return stdev(lst) if len(lst) > 1 else 0


def save_digest(summary_results, save_digest_path_prefix, note=""):
    detail_args_list = summary_results[list(summary_results.keys())[0]].keys()
    for detail_arg in detail_args_list:
        run_threads = set()
        for model_results in summary_results.values():
            run_threads.update(model_results[detail_arg].keys())
        run_threads = sorted(list(run_threads))

        save_summary_name = save_summary_format.format(
            params=detail_arg.replace(" ", "_"),
            note=note,
        )
        save_summary_path = os.path.join(
            save_digest_path_prefix,
            save_summary_name,
        )
        os.makedirs(os.path.dirname(save_summary_path), exist_ok=True)

        with open(save_summary_path, "w") as f:
            f.write("model_type,threads,throughput\n")
            for model_type, model_results in summary_results.items():
                for thread, throughput in model_results[detail_arg].items():
                    f.write(f"{model_type},{thread},{throughput}\n")

        save_plotdata_name = save_plotdata_format.format(
            params=detail_arg.replace(" ", "_"),
            note=note,
        )
        save_plotdata_path = os.path.join(
            save_digest_path_prefix,
            save_plotdata_name,
        )
        os.makedirs(os.path.dirname(save_plotdata_path), exist_ok=True)

        with open(save_plotdata_path, "w") as f:
            first_line = (detail_arg.replace(" ", "_"), *summary_results.keys())
            f.write(",".join(first_line) + "\n")
            for thread in run_threads:
                line = [thread]
                for model_type, model_results in summary_results.items():
                    line.append(model_results[detail_arg].get(thread, 0))
                f.write(",".join(map(str, line)) + "\n")

        print(
            f"Saved summary and plotdata at {save_summary_path} and {save_plotdata_path}"
        )


def load_raw_results(raw_results_path, note=""):
    summary_results = {}
    for file_name in os.listdir(raw_results_path):
        if note != "" and note not in file_name:
            continue
        if not file_name.endswith(".csv") or "aux_" in file_name:
            continue

        file_path = os.path.join(raw_results_path, file_name)
        with open(file_path, "r") as f:
            detail_arg, token = file_name.split("__")
            model_type, _ = token.split(".csv")
            model_type = model_type.strip("_")

            if model_type not in summary_results:
                summary_results[model_type] = {}

            if detail_arg not in summary_results[model_type]:
                summary_results[model_type][detail_arg] = {}

            summary_results[model_type][detail_arg] = defaultdict(
                lambda: {
                    "throughput": [],
                    "fairness": [],
                    "max_access_ratio": [],
                    "root_access_ratio": [],
                }
            )
            my_result_dict = summary_results[model_type][detail_arg]

            for line in f:
                tokens = line.split(",")
                thread = int(tokens[0])
                throughput = float(tokens[-1])
                fairness = float(tokens[-3])
                root_access_ratio = float(tokens[-4])
                max_access_ratio = float(tokens[-5])

                my_result_dict[thread]["throughput"].append(throughput)
                my_result_dict[thread]["fairness"].append(fairness)
                my_result_dict[thread]["root_access_ratio"].append(root_access_ratio)
                my_result_dict[thread]["max_access_ratio"].append(max_access_ratio)

            for thread, results in my_result_dict.items():
                res = my_result_dict[thread]
                res["throughput_stdev"] = get_stddev(results["throughput"])
                res["throughput"] = get_avg(results["throughput"])
                res["fairness_stdev"] = get_stddev(results["fairness"])
                res["fairness"] = get_avg(results["fairness"])
                res["root_access_ratio_stdev"] = get_stddev(
                    results["root_access_ratio"]
                )
                res["root_access_ratio"] = get_avg(results["root_access_ratio"])
                res["max_access_ratio_stdev"] = get_stddev(results["max_access_ratio"])
                res["max_access_ratio"] = get_avg(results["max_access_ratio"])

            print(f"Loaded {file_path}")

    return summary_results


# add aux loading


def plot_series(summary_results, save_path, series_name, stdev_name=None):
    plt.rcParams.update({"font.size": 15})

    markers = ["o", "s", "p", "P", "*", "h", "H", "+", "x", "X", "D", "d", "|", "_"]

    detail_args_list = summary_results[list(summary_results.keys())[0]].keys()
    for detail_arg in detail_args_list:
        run_threads = set()
        for model_results in summary_results.values():
            run_threads.update(model_results[detail_arg].keys())
        run_threads = sorted(list(run_threads))

        fig, ax = plt.subplots(figsize=(10, 6))
        for idx, (model_type, model_results) in enumerate(summary_results.items()):
            avg_throughputs = [
                model_results[detail_arg].get(thread, {}).get(series_name, 0)
                for thread in run_threads
            ]
            if stdev_name is None:
                lines = ax.plot(run_threads, avg_throughputs, label=model_type)
                for line in lines:
                    line.set_marker(markers[idx % len(markers)])
                    line.set_markersize(5)
                    line.set_color(plt.cm.tab20(idx))

            else:
                stdev_throughputs = [
                    model_results[detail_arg].get(thread, {}).get(stdev_name, 0)
                    for thread in run_threads
                ]
                line, caps, bars = ax.errorbar(
                    run_threads,
                    avg_throughputs,
                    yerr=stdev_throughputs,
                    label=model_type,
                    capsize=3,
                )
                line.set_marker(markers[idx % len(markers)])
                line.set_markersize(5)
                line.set_color(plt.cm.tab20(idx))
                for cap in caps:
                    cap.set_color(plt.cm.tab20(idx))
                for bar in bars:
                    bar.set_color(plt.cm.tab20(idx))

        ax.set(xlabel="Threads", ylabel=series_name, title=detail_arg)
        ax.grid()
        # ax.set_ylim(bottom=0)

        ax.legend(bbox_to_anchor=(1.05, 1), loc="upper left")

        # save as pdf
        os.makedirs(save_path, exist_ok=True)
        # save_plot_path = os.path.join(save_path, f"{detail_arg}.png")
        # fig.savefig(save_plot_path)
        # save_plot_path = os.path.join(save_path, f"{detail_arg}.svg")
        # fig.savefig(save_plot_path)
        save_plot_path = os.path.join(save_path, f"{detail_arg}_{series_name}.pdf")
        fig.savefig(save_plot_path)
        plt.close(fig)

        print(f"Saved plot at {save_plot_path}")


def get_args():
    parser = argparse.ArgumentParser(description="Digest benchmark results")
    parser.add_argument(
        "--mode",
        type=str,
        required=True,
        choices=["plot", "digest"],
        help="Mode to run the digester",
    )

    parser.add_argument(
        "--results_path",
        type=str,
        required=True,
        help="Path to the summary results file (e.g. results/counter/20240726T13)",
    )
    parser.add_argument(
        "--note",
        type=str,
        default="",
        help="Note to add to the digested results",
    )
    return parser.parse_args()


def main():
    args = get_args()
    results_path = args.results_path
    note = args.note

    raw_results_path = os.path.join(results_path, "raw")
    summary_results = load_raw_results(raw_results_path, note)

    if args.mode == "digest":
        digest_path = os.path.join(results_path, "digest")
        save_digest(summary_results, digest_path, note)
    elif args.mode == "plot":
        plot_path = os.path.join(results_path, "plot")
        # plot_summary(summary_results, plot_path)
        plot_series(summary_results, plot_path, "throughput", "throughput_stdev")
        plot_series(summary_results, plot_path, "fairness")
        plot_series(summary_results, plot_path, "root_access_ratio")
        plot_series(summary_results, plot_path, "max_access_ratio")

    print("Done")


if __name__ == "__main__":
    main()
