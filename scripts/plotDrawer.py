import os
import pandas as pd
import plotly.express as px
import matplotlib.pyplot as plt
from pprint import pprint
import argparse

## default setting
plt.rcParams["pdf.fonttype"] = 42
plt.rcParams["ps.fonttype"] = 42
# set font size
plt.rcParams["font.size"] = 28
# set marker size
plt.rcParams["lines.markersize"] = 15


## plot names and legends
def default_agg_spec(name):
    return {
        "marker": "o",
        "color": "tab:blue",
        "name": name,
        "zorder": 20,
    }


def default_hw_spec(name="HardwareF&A"):
    return {
        "marker": "^",
        "color": "tab:orange",
        "name": name,
        "zorder": 10,
    }


fig3_legends = {
    "hardwareCounter": default_hw_spec(),
    "confRootAggFunnelCounter": {
        "marker": "d",
        "color": "tab:green",
        "name": "AggFunnel-sqrt(p)",
    },
    "configuredAggFunnelCounter_4": {
        "marker": "v",
        "color": "tab:purple",
        "name": "AggFunnel-4",
        "zorder": 4,
    },
    "configuredAggFunnelCounter_6": default_agg_spec("AggFunnel-6"),
    "configuredAggFunnelCounter_8": {
        "marker": ">",
        "color": "tab:red",
        "name": "AggFunnel-8",
        "zorder": 5,
    },
    "configuredAggFunnelCounter_10": {
        "marker": "X",
        "color": "tab:cyan",
        "name": "AggFunnel-10",
    },
}

fig4_legends = {
    "hardwareCounter": default_hw_spec(),
    "aggFunnelCounter": default_agg_spec("AggFunnel"),
    "combFunnelCounter": {
        "marker": "x",
        "color": "tab:purple",
        "name": "CombFunnel",
        "zorder": 20,
    },
    "recursiveAggFunnelCounter": {
        "marker": "v",
        "color": "tab:brown",
        "name": "RecursiveAggFunnel",
        "zorder": 20,
    },
}

fig5_legends = {
    "hardwareCounter": default_hw_spec(),
    "configuredAggFunnelCounter_6_0": default_agg_spec("AggFunnel-(6,0)"),
    "configuredAggFunnelCounter_6_1": {
        "marker": ">",
        "color": "tab:cyan",
        "name": "AggFunnel-(6,1)",
        "zorder": 40,
    },
    "configuredAggFunnelCounter_6_2": {
        "marker": "x",
        "color": "tab:olive",
        "name": "AggFunnel-(6,2)",
        "zorder": 35,
    },
    "configuredAggFunnelCounter_2_0": {
        "marker": "X",
        "color": "tab:red",
        "name": "AggFunnel-(2,0)",
        "zorder": 30,
    },
    "configuredAggFunnelCounter_2_1": {
        "marker": "v",
        "color": "tab:green",
        "name": "AggFunnel-(2,1)",
        "zorder": 40,
    },
    "configuredAggFunnelCounter_2_2": {
        "marker": "d",
        "color": "tab:purple",
        "name": "AggFunnel-(2,2)",
        "zorder": 35,
    },
}

fig6_legends = {
    "LCRQueue/remap/SimpleAtomicCounter": default_hw_spec("LCRQ-HardwareF&A"),
    "LCRQueue/remap/AggFunnelCounter": default_agg_spec("LCRQ-AggFunnel"),
    "LPRQueue/remap/SimpleAtomicCounter": {
        "marker": "d",
        "color": "tab:cyan",
        "name": "LPRQ-HardwareF&A",
        "zorder": 5,
    },
    "LCRQueue/remap/CombiningFunnelCounter": {
        "marker": "x",
        "color": "tab:purple",
        "name": "LCRQ-CombFunnel",
        "zorder": 15,
    },
    "LSCQueue/remap/SimpleAtomicCounter": {
        "marker": "s",
        "color": "tab:red",
        "name": "LSCQ-HardwareF&A",
        "zorder": 5,
    },
}


def load_and_prepare_main_df(data_path):
    main_path = data_path + "/main.csv"
    str_cols = [
        "model",
        "build_params",
        "exec_params",
    ]
    int_cols = [
        "thread_count",
        "rep",
        "run_milliseconds",
        "read_percent",
        "increment_percent",
        "additional_work",
    ]
    float_cols = [
        "max_access_ratio",
        "root_access_ratio",
        "fairness",
        "throughput",
    ]
    main_df = pd.read_csv(main_path, usecols=str_cols + int_cols + float_cols)
    main_df["build_params"] = main_df["build_params"].fillna("")
    main_df[int_cols] = main_df[int_cols].astype(int)
    main_df[float_cols] = main_df[float_cols].astype(float)
    main_df["batch_size"] = 1 / main_df["root_access_ratio"]
    return main_df


def load_and_prepare_aux_df(data_path):
    main_path = data_path + "/aux.csv"
    str_cols = [
        "model",
        "build_params",
        "exec_params",
    ]
    int_cols = [
        "thread_id",
        "threads",
        "rep",
        "read_count",
        "inc_count",
        "total_count",
    ]

    aux_df = pd.read_csv(main_path, usecols=str_cols + int_cols)
    aux_df["build_params"] = aux_df["build_params"].fillna("")
    aux_df[int_cols] = aux_df[int_cols].astype(int)
    aux_df = aux_df.rename(columns={"threads": "thread_count"})
    return aux_df


def join_hi_lo_ratio(main_df, aux_df):
    common_cols = ["model", "build_params", "exec_params", "thread_count", "rep"]
    for vals, cur_df in aux_df.groupby(common_cols):
        _, build_params, _, _, _ = vals
        agg_count, direct_count = parse_build_params(build_params)
        if direct_count == 0 or direct_count is None:
            hi_lo_ratio = 0
        elif direct_count == 1:
            hi_lo_ratio = cur_df[cur_df["thread_id"] == 0]["total_count"].mean() / (
                cur_df[cur_df["thread_id"] != 0]["total_count"].mean()
            )
        elif direct_count == 2:
            hi_lo_ratio = cur_df[cur_df["thread_id"] <= 1]["total_count"].mean() / (
                cur_df[cur_df["thread_id"] > 1]["total_count"].mean()
            )
        if hi_lo_ratio != hi_lo_ratio:
            hi_lo_ratio = 0
        aux_df.loc[cur_df.index, "hi_lo_ratio"] = hi_lo_ratio

    joined = main_df.merge(aux_df, on=common_cols)
    return joined


def group_metrics(main_df):
    # get a mean and stdev of throughput and fairness columns, grouped by model, exec_params, and thread_count
    # and return joined dataframe
    common_cols = ["model", "build_params", "exec_params", "thread_count"]
    grouped = main_df.groupby(common_cols)
    throughput_df = grouped["throughput"].agg(["mean", "std"]).reset_index()
    throughput_df = throughput_df.rename(
        columns={"mean": "throughput_mean", "std": "throughput_std"}
    )
    fairness_df = grouped["fairness"].agg(["mean", "std"]).reset_index()
    fairness_df = fairness_df.rename(
        columns={"mean": "fairness_mean", "std": "fairness_std"}
    )
    batch_size_df = grouped["batch_size"].agg(["mean"]).reset_index()
    batch_size_df = batch_size_df.rename(columns={"mean": "batch_size_mean"})
    # if hi_lo_ratio is not calculated, set it to 0
    if "hi_lo_ratio" not in main_df.columns:
        main_df["hi_lo_ratio"] = 0
    ratio_df = grouped["hi_lo_ratio"].agg(["mean"]).reset_index()
    ratio_df = ratio_df.rename(columns={"mean": "hi_lo_ratio_mean"})
    joined = throughput_df.merge(fairness_df, on=common_cols)
    joined = joined.merge(batch_size_df, on=common_cols)
    joined = joined.merge(ratio_df, on=common_cols)
    return joined


def parse_build_params(build_params):
    vars = build_params.split(" ")
    agg_count = None
    DIRECT_COUNT = None
    for var in vars:
        tokens = var.split("=")
        if tokens[0] == "AGG_COUNT":
            agg_count = int(tokens[1])
        elif tokens[0] == "DIRECT_COUNT":
            DIRECT_COUNT = int(tokens[1])
    return agg_count, DIRECT_COUNT


def draw_legends(file_path, legend_specs):
    fig, ax = plt.subplots(figsize=(25, 1.8))
    for key, spec in legend_specs.items():
        ax.plot(
            [],
            [],
            spec["marker"],
            color=spec["color"],
            label=spec["name"],
            linestyle="solid",
        )
    ax.legend()
    ax.axis("off")

    handles, labels = ax.get_legend_handles_labels()
    plt.legend(
        handles,
        labels,
        loc="upper left",
        bbox_to_anchor=(0.075, 1),
        frameon=False,
        ncol=4,
    )
    # plt.show()
    print(f"Saving legends to {file_path}")
    plt.savefig(file_path, bbox_inches="tight")


def draw_plot(
    file_path,
    df,
    x_col,
    y_col,
    y_err,
    legend_specs,
    title,
    x_label,
    y_label,
    key_format="{model}",
    y_multiplier=1,
):
    fig, ax = plt.subplots(figsize=(10, 7))
    for (model, build_params), model_df in df.groupby(["model", "build_params"]):
        agg_count, direct_count = parse_build_params(build_params)
        key = key_format.format(
            model=model, agg_count=agg_count, direct_count=direct_count
        )
        if "None" in key:
            key = model
        spec = legend_specs[key]
        marker = spec["marker"]
        color = spec["color"]
        name = spec["name"]
        zorder = spec.get("zorder", 0)
        linestyle = spec.get("linestyle", "solid")

        line, caps, bars = plt.errorbar(
            model_df[x_col],
            model_df[y_col] * y_multiplier,
            yerr=model_df[y_err] * y_multiplier if y_err else None,
            label=name,
            marker=marker,
            zorder=zorder,
            color=color,
            linestyle=linestyle,
            capsize=3,
        )

        plt.setp(line, marker=marker)

    plt.xlabel(x_label)
    plt.ylabel(y_label)
    plt.title(title, loc="center", pad=20)

    plt.xlim(left=-1)
    plt.ylim(bottom=-0.1)
    ax.xaxis.grid(True, linestyle="dotted")
    ax.yaxis.grid(True, linestyle="dotted")

    # plt.show()
    print(f"Saving plot to {file_path}")
    plt.savefig(file_path, bbox_inches="tight")


def draw_figure_3_plots(df_path, save_path):
    df = load_and_prepare_main_df(df_path)
    df = group_metrics(df)

    legend_path = save_path + "/legends.png"
    draw_legends(legend_path, fig3_legends)

    fig3a_df = df[df["exec_params"] == "10 90 32"]
    fig3a_path = save_path + "/fig3a.png"
    draw_plot(
        fig3a_path,
        fig3a_df,
        "thread_count",
        "throughput_mean",
        "throughput_std",
        fig3_legends,
        "3a: 90% Fetch&Add, throughput",
        "Number of threads",
        "Throughput (Mops/s)",
        key_format="{model}_{agg_count}",
        y_multiplier=1e-3,
    )

    fig3b_df = fig3a_df
    fig3b_path = save_path + "/fig3b.png"
    draw_plot(
        fig3b_path,
        fig3b_df,
        "thread_count",
        "batch_size_mean",
        None,
        fig3_legends,
        "3b: 90% Fetch&Add, batch size",
        "Number of threads",
        "Average batch size",
        key_format="{model}_{agg_count}",
    )

    fig3c_df = df[df["exec_params"] == "50 50 32"]
    fig3c_path = save_path + "/fig3c.png"
    draw_plot(
        fig3c_path,
        fig3c_df,
        "thread_count",
        "throughput_mean",
        "throughput_std",
        fig3_legends,
        "3c: 50% Fetch&Add, throughput",
        "Number of threads",
        "Throughput (Mops/s)",
        key_format="{model}_{agg_count}",
        y_multiplier=1e-3,
    )

    print("Figure 3 plots are saved.")
    return


def draw_figure_4_plots(df_path, save_path):
    df = load_and_prepare_main_df(df_path)
    df = group_metrics(df)

    legend_path = save_path + "/legends.png"
    draw_legends(legend_path, fig4_legends)

    def draw_throughput_plot(path, df, title):
        draw_plot(
            path,
            df,
            "thread_count",
            "throughput_mean",
            "throughput_std",
            fig4_legends,
            title,
            "Number of threads",
            "Throughput (Mops/s)",
            y_multiplier=1e-3,
        )

    fig4a_df = df[df["exec_params"] == "10 90 32"]
    fig4a_path = save_path + "/fig4a.png"
    draw_throughput_plot(
        fig4a_path, fig4a_df, "4a: 90% Fetch&Add, 512 cycles, throughput"
    )

    fig4b_df = fig4a_df
    fig4b_path = save_path + "/fig4b.png"
    draw_plot(
        fig4b_path,
        fig4b_df,
        "thread_count",
        "fairness_mean",
        "fairness_std",
        fig4_legends,
        "4b: 90% Fetch&Add, 512 cycles, fairness",
        "Number of threads",
        "Fairness",
    )

    fig4c_df = df[df["exec_params"] == "10 90 2"]
    fig4c_path = save_path + "/fig4c.png"
    draw_throughput_plot(
        fig4c_path, fig4c_df, "4c: 90% Fetch&Add, 32 cycles, throughput"
    )

    fig4d_df = df[df["exec_params"] == "0 100 32"]
    fig4d_path = save_path + "/fig4d.png"
    draw_throughput_plot(fig4d_path, fig4d_df, "4d: 100% Fetch&Add, throughput")

    fig4e_df = df[df["exec_params"] == "50 50 32"]
    fig4e_path = save_path + "/fig4e.png"
    draw_throughput_plot(fig4e_path, fig4e_df, "4e: 50% Fetch&Add, throughput")

    fig4f_df = df[df["exec_params"] == "90 10 32"]
    fig4f_path = save_path + "/fig4f.png"
    draw_throughput_plot(fig4f_path, fig4f_df, "4f: 10% Fetch&Add, throughput")

    print("Figure 4 plots are saved.")
    return


def draw_figure_5_plots(df_path, save_path):
    main_df = load_and_prepare_main_df(df_path)
    aux_df = load_and_prepare_aux_df(df_path)
    joined_df = join_hi_lo_ratio(main_df, aux_df)
    grouped_df = group_metrics(joined_df)

    legend_path = save_path + "/legends.png"
    draw_legends(legend_path, fig5_legends)

    fig5a_df = grouped_df[grouped_df["exec_params"] == "10 90 2"]
    fig5a_path = save_path + "/fig5a.png"
    draw_plot(
        fig5a_path,
        fig5a_df,
        "thread_count",
        "throughput_mean",
        "throughput_std",
        fig5_legends,
        "5a: throughput",
        "Number of threads",
        "Throughput (Mops/s)",
        key_format="{model}_{agg_count}_{direct_count}",
        y_multiplier=1e-3,
    )

    fig5b_df = fig5a_df[
        (fig5a_df["model"] == "hardwareCounter")
        | ~fig5a_df["build_params"].str.contains("DIRECT_COUNT=0")
    ]
    fig5b_path = save_path + "/fig5b.png"
    draw_plot(
        fig5b_path,
        fig5b_df,
        "thread_count",
        "hi_lo_ratio_mean",
        None,
        fig5_legends,
        "5b: Average throughput ratio",
        "Number of threads",
        "High / Low Ratio",
        key_format="{model}_{agg_count}_{direct_count}",
    )

    fig5c_df = fig5a_df
    fig5c_path = save_path + "/fig5c.png"
    draw_plot(
        fig5c_path,
        fig5c_df,
        "thread_count",
        "batch_size_mean",
        None,
        fig5_legends,
        "5c: batch size",
        "Number of threads",
        "Average batch size",
        key_format="{model}_{agg_count}_{direct_count}",
    )

    print("Figure 5 plots are saved.")
    return


def load_queue_df(data_path):
    main_path = data_path + "/main.csv"
    str_cols_map = {
        "Benchmark": "task",
        "Param: queueType": "model",
    }
    int_cols_map = {
        "Threads": "thread_count",
        "Param: additionalWork": "additional_work",
        "Param: ringSize": "ring_size",
        "Score": "throughput_mean",
        "Score Error": "throughput_std",
    }
    cols = list(str_cols_map.keys()) + list(int_cols_map.keys())
    main_df = pd.read_csv(main_path, usecols=cols)
    main_df[list(int_cols_map.keys())] = main_df[list(int_cols_map.keys())].astype(int)
    main_df = main_df.rename(columns=str_cols_map)
    main_df = main_df.rename(columns=int_cols_map)
    # split Benchmark into exec_params
    main_df["exec_params"] = main_df["task"].str.split("_").str[-1]

    return main_df


def draw_figure_6_plots(df_path, save_path):
    df = load_queue_df(df_path)
    legend_path = save_path + "/legends.png"
    draw_legends(legend_path, fig6_legends)

    fig6a_df = df
    fig6a_path = save_path + "/fig6a.png"
    draw_plot(
        fig6a_path,
        fig6a_df,
        "thread_count",
        "throughput_mean",
        "throughput_std",
        fig6_legends,
        "6a: Pairwise enq-deq",
        "Number of threads",
        "Throughput (Mops/s)",
        key_format="{model}",
        y_multiplier=1e-6,
    )

    print("Figure 6 plots are saved.")
    return


def draw_example_plots(df_path, save_path):
    df = load_and_prepare_main_df(df_path)
    df = group_metrics(df)

    legend_path = save_path + "/legends.png"
    draw_legends(legend_path, fig4_legends)

    figa_df = df[df["exec_params"] == "10 90 32"]
    figa_path = save_path + "/figa.png"
    draw_plot(
        figa_path,
        figa_df,
        "thread_count",
        "throughput_mean",
        None,
        fig4_legends,
        "Example: 90% Fetch&Add, 512 cycles, throughput",
        "Number of threads",
        "Throughput (Mops/s)",
        y_multiplier=1e-3,
    )

    figb_df = df[df["exec_params"] == "90 10 32"]
    figb_path = save_path + "/figb.png"
    draw_plot(
        figb_path,
        figb_df,
        "thread_count",
        "throughput_mean",
        None,
        fig4_legends,
        "Example: 10% Fetch&Add, 512 cycles, throughput",
        "Number of threads",
        "Throughput (Mops/s)",
        y_multiplier=1e-3,
    )

    print("Example plots are saved.")
    return


def main():
    parser = argparse.ArgumentParser(description="Draw plots for the benchmark results")
    parser.add_argument(
        "--figure_num",
        type=str,
        default="4",
        required=True,
        choices=["3", "4", "5", "6", "ex"],
        help="Figure number to draw",
    )
    parser.add_argument(
        "--data_path",
        type=str,
        required=True,
        help="Path to the benchmark results",
    )
    parser.add_argument(
        "--save_path",
        type=str,
        default="./results/plots",
        help="Path to save the plots",
    )

    args = parser.parse_args()
    figure_num = args.figure_num
    data_path = args.data_path
    save_path = args.save_path
    if save_path == "./results/plots":
        save_path += f"/figure{figure_num}"
    os.makedirs(save_path, exist_ok=True)

    print(f"Drawing plots for figure {figure_num}...")

    if figure_num == "3":
        draw_figure_3_plots(data_path, save_path)
    elif figure_num == "4":
        draw_figure_4_plots(data_path, save_path)
    elif figure_num == "5":
        draw_figure_5_plots(data_path, save_path)
    elif figure_num == "6":
        draw_figure_6_plots(data_path, save_path)
    elif figure_num == "ex":
        draw_example_plots(data_path, save_path)


if __name__ == "__main__":
    main()
