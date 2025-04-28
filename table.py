#!/usr/bin/env python3


import subprocess
import os
import datetime
from collections import defaultdict
from tabulate import tabulate


log_file = "logs/table.log"
image_tag = "reproduce-deepstream-segfault-c"
thread_range = range(1, 13)
encoders_range = range(1, 13)

# Create a nested dictionary to store results
# {thread_count: {encoders_per_pipeline: [list of exit codes]}}
results_table = {t: {e: [] for e in encoders_range} for t in thread_range}


os.makedirs("logs", exist_ok=True)


def log_and_print(message):
    timestamp = datetime.datetime.now().strftime("%H:%M:%S")
    full_message = f"{timestamp} {message}"
    print(full_message)
    with open(log_file, "a") as f:
        f.write(full_message + "\n")


def print_results_table():
    # Prepare table data
    headers = ["Thr \\ Enc"] + list(encoders_range)
    table_data = []

    for t in thread_range:
        row = [t]
        for e in encoders_range:
            exit_codes = results_table[t][e]
            if not exit_codes:
                cell_content = "{}"
            else:
                # Count occurrences of each exit code
                counts = defaultdict(int)
                for code in exit_codes:
                    counts[code] += 1
                cell_content = (
                    "{"
                    + ", ".join(
                        [f"{code}: {count}" for code, count in sorted(counts.items())]
                    )
                    + "}"
                )
            row.append(cell_content)
        table_data.append(row)

    # Print the table
    print(f"\nResults Table (iteration {iteration})")
    table = tabulate(table_data, headers=headers, tablefmt="grid")
    log_and_print(f"\n{table}")
    print()


# Build the Docker image
log_and_print("Building Docker image...")
subprocess.run(["docker", "build", "-t", image_tag, "."], check=True)

# Remove existing log file
try:
    os.remove(log_file)
except FileNotFoundError:
    pass

log_and_print("Starting test iterations...")

iteration = 0
try:
    while True:
        iteration += 1
        log_and_print(f"Starting iteration {iteration}...")

        # First complete one full grid before repeating combinations
        max_runs_per_cell = iteration

        for thread_count in thread_range:
            for encoders_per_pipeline in encoders_range:
                # Check if we need to run this cell in this iteration
                if (
                    len(results_table[thread_count][encoders_per_pipeline])
                    < max_runs_per_cell
                ):
                    log_and_print(
                        f"Running with THREAD_COUNT={thread_count}, ENCODERS_PER_PIPELINE={encoders_per_pipeline}"
                    )

                    # Run the Docker container
                    result = subprocess.run(
                        [
                            "docker",
                            "run",
                            "--rm",
                            "--gpus",
                            "all",
                            "--env",
                            "GST_DEBUG=2",
                            "--env",
                            f"THREAD_COUNT={thread_count}",
                            "--env",
                            f"ENCODERS_PER_PIPELINE={encoders_per_pipeline}",
                            image_tag,
                            "/app/src/pipeline_test",
                        ],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                    )

                    # Write output to log file
                    with open(log_file, "a") as f:
                        f.write(result.stdout)

                    # Store exit code in results table
                    exit_code = result.returncode
                    results_table[thread_count][encoders_per_pipeline].append(exit_code)

                    # Log the result
                    log_and_print(
                        f"Exited with code {exit_code} | TC={thread_count}, EPP={encoders_per_pipeline}"
                    )

                    # Print the updated table after each complete iteration
                    print_results_table()


except KeyboardInterrupt:
    print("\nStopped by user")
    print_results_table()  # Print final results
