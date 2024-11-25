import pandas as pd
import plotly.express as px
import matplotlib.pyplot as plt
from pprint import pprint

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


default_hw_spec = {
    "marker": "^",
    "color": "tab:orange",
    "name": "HardwareF&A",
    "zorder": 10,
}

fig3_legends = {
    "hardwareCounter": default_hw_spec,
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
    "hardwareCounter": default_hw_spec,
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
    "hardwareCounter": default_hw_spec,
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


def load_and_normalize_main_df(data_path):
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
    return main_df


def group_throughput_and_fairness(main_df):
    # get a mean and stdev of throughput and fairness columns, grouped by model, exec_params, and thread_count
    # and return joined dataframe
    grouped = main_df.groupby(["model", "exec_params", "thread_count"])
    throughput_df = grouped["throughput"].agg(["mean", "std"]).reset_index()
    throughput_df = throughput_df.rename(
        columns={"mean": "throughput_mean", "std": "throughput_std"}
    )
    fairness_df = grouped["fairness"].agg(["mean", "std"]).reset_index()
    fairness_df = fairness_df.rename(
        columns={"mean": "fairness_mean", "std": "fairness_std"}
    )
    joined = throughput_df.merge(
        fairness_df, on=["model", "exec_params", "thread_count"]
    )
    return joined


def draw_legends(legend_specs):
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
    plt.show()


# cur_df = df[df["detail_arg"] == "90_10_32"]
# cur_df = cur_df[cur_df["model"].isin(model_filter)]
# cur_df = cur_df.sort_values(by=["detail_arg", "model", "n"])


# fig, ax = plt.subplots(figsize=(10, 7))
# for model, model_df in cur_df.groupby("model"):
#     marker = model_info[model]["marker"]
#     color = model_info[model]["color"]
#     name = model_info[model]["name"]
#     zorder = model_info[model].get("zorder", 0)

#     line, caps, bars = plt.errorbar(
#         model_df["n"],
#         model_df["throughput"] / 1e6,
#         yerr=model_df["throughput_stdev"] / 1e6,
#         label=name,
#         marker=marker,
#         zorder=zorder,
#         color=color,
#         capsize=3,
#     )

#     # Set line markers
#     plt.setp(line, marker=marker)
#     # plt.setp(caps, marker=marker)

# plt.xlabel("Number of threads")
# plt.ylabel("Average Batch Size")

# plt.xlim(left=-1)
# plt.ylim(bottom=-0.1)
# ax.xaxis.grid(True, linestyle="dotted")
# ax.yaxis.grid(True, linestyle="dotted")


def draw_plot(
    df,
    x_col,
    y_col,
    y_err,
    hue_col,
    legend_specs,
    title,
    x_label,
    y_label,
    y_multiplier=1,
):
    fig, ax = plt.subplots(figsize=(10, 7))
    for model, model_df in df.groupby(hue_col):
        spec = legend_specs[model]
        marker = spec["marker"]
        color = spec["color"]
        name = spec["name"]
        zorder = spec.get("zorder", 0)
        linestyle = spec.get("linestyle", "solid")

        line, caps, bars = plt.errorbar(
            model_df[x_col],
            model_df[y_col] * y_multiplier,
            yerr=model_df[y_err] * y_multiplier,
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

    plt.show()
