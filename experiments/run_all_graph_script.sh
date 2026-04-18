#!/bin/bash

# Loop over all items inside experiments/
for dir in */; do
    # Check if it's a directory and contains run_script.sh
    if [ -f "$dir/run_script.sh" ]; then
        echo "Processing $dir"

        # Enter the directory
        cd "$dir" || continue

        cd graph_generation
        python3 ./generate_graph_prefix_sum_logs.py 44991

        # Go back to experiments/
        cd ../../
    else
        echo "Skipping $dir (no run_script.sh found)"
    fi
done