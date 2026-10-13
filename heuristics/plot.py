import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def plot_allocations(
    filename,
    output="gpu_allocations.png"
):

    # =========================================================
    # Read allocation CSV
    # =========================================================

    df = pd.read_csv(filename)

    df["job_id"] = pd.to_numeric(
        df["job_id"]
    ).astype(int)

    df["iteration"] = pd.to_numeric(
        df["iteration"]
    ).astype(int)

    df["start"] = pd.to_numeric(
        df["start"]
    )

    df["end"] = pd.to_numeric(
        df["end"]
    )

    # =========================================================
    # Fix reversed start/end if necessary
    # =========================================================

    n_reversed = (
        df["end"] < df["start"]
    ).sum()

    n_valid = (
        df["end"] >= df["start"]
    ).sum()

    if n_reversed > n_valid:

        print(
            "WARNING: start/end appear to be reversed."
        )

        old_start = df["start"].copy()

        df["start"] = df["end"]
        df["end"] = old_start

    # =========================================================
    # Determine GPUs
    # =========================================================

    gpu_ids = sorted(
        df["gpus"]
        .astype(str)
        .str.split(";")
        .explode()
        .astype(int)
        .unique()
    )

    R = len(gpu_ids)

    gpu_to_row = {
        gpu: r
        for r, gpu in enumerate(gpu_ids)
    }

    # =========================================================
    # Determine jobs
    # =========================================================

    jobs = sorted(
        df["job_id"].unique()
    )

    # Map arbitrary job IDs to consecutive values.
    #
    # This is important because imshow works with numerical
    # values and the default colormap is indexed by those values.
    #
    # Idle = -1
    # Job 0 = 0
    # Job 1 = 1
    # etc.
    #
    # The actual job IDs are still used for labels.
    # =========================================================

    job_to_index = {
        job: i
        for i, job in enumerate(jobs)
    }

    # =========================================================
    # Makespan
    # =========================================================

    makespan = int(
        df["end"].max()
    )

    # Same margin as your original plot
    margin = max(
        20,
        int(0.05 * makespan)
    )

    plot_end = makespan + margin

    print(
        f"Makespan: {makespan}"
    )

    print(
        f"Plotting time range: 0 - {plot_end}"
    )

    # =========================================================
    # Occupancy matrix
    #
    # -1 = idle
    #  0 = first job
    #  1 = second job
    # ...
    # =========================================================

    occupancy = -np.ones(
        (R, plot_end),
        dtype=int
    )

    # =========================================================
    # Fill occupancy matrix
    # =========================================================

    for _, row in df.iterrows():

        job = int(
            row["job_id"]
        )

        start = int(
            row["start"]
        )

        end = int(
            row["end"]
        )

        # Ignore zero-duration iterations
        if end <= start:
            continue

        # Clip to plotting range
        start_plot = max(
            0,
            start
        )

        end_plot = min(
            plot_end,
            end
        )

        if start_plot >= end_plot:
            continue

        # GPUs allocated to this iteration
        gpus = [
            int(x)
            for x in str(
                row["gpus"]
            ).split(";")
            if x != ""
        ]

        for gpu in gpus:

            r = gpu_to_row[gpu]

            occupancy[
                r,
                start_plot:end_plot
            ] = job_to_index[job]

    # =========================================================
    # Plot
    #
    # This is intentionally the SAME plotting mechanism as
    # your original CP-SAT plot.
    # =========================================================

    fig, ax = plt.subplots(
        figsize=(16, 6)
    )

    ax.imshow(
        occupancy,
        aspect="auto",
        interpolation="none",
        extent=[
            0,
            plot_end,
            R - 0.5,
            -0.5
        ]
    )

    ax.set_xlabel(
        "Time step"
    )

    ax.set_ylabel(
        "GPU"
    )

    ax.set_yticks(
        range(R)
    )

    ax.set_yticklabels(
        [
            f"GPU {gpu}"
            for gpu in gpu_ids
        ]
    )

    ax.set_title(
        f"GPU Allocation (makespan = {makespan})"
    )

    # =========================================================
    # Vertical grid lines
    # =========================================================

    number_of_ticks = 20

    ax.set_xticks(
        np.linspace(
            0,
            plot_end,
            number_of_ticks + 1
        )
    )

    ax.grid(
        axis="x",
        linestyle="--",
        alpha=0.4
    )

    # =========================================================
    # Add job / iteration labels
    # =========================================================

    for _, row in df.iterrows():

        job = int(
            row["job_id"]
        )

        iteration = int(
            row["iteration"]
        )

        start = int(
            row["start"]
        )

        end = int(
            row["end"]
        )

        if start >= plot_end:
            continue

        if end <= start:
            continue

        end_plot = min(
            end,
            plot_end
        )

        visible_width = (
            end_plot - start
        )

        if visible_width < 3:
            continue

        gpus = [
            int(x)
            for x in str(
                row["gpus"]
            ).split(";")
            if x != ""
        ]

        for gpu in gpus:

            r = gpu_to_row[gpu]

            ax.text(
                (start + end_plot) / 2,
                r,
                f"J{job}-I{iteration}",
                ha="center",
                va="center",
                fontsize=8
            )

    # =========================================================
    # Mark makespan
    # =========================================================

    ax.axvline(
        makespan,
        linestyle="--",
        linewidth=1.5
    )

    ax.text(
        makespan,
        R - 0.5,
        f"  makespan = {makespan}",
        rotation=90,
        va="bottom",
        ha="left"
    )

    # =========================================================
    # Save
    # =========================================================

    plt.tight_layout()

    plt.savefig(
        output,
        dpi=300,
        bbox_inches="tight"
    )

    plt.close()

    print(
        f"Figure saved to: {output}"
    )


if __name__ == "__main__":

    plot_allocations(
        sys.argv[1],
        "gpu_allocations.png"
    )
