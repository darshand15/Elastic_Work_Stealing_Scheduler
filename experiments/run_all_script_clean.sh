#!/bin/bash

# Loop over all items inside experiments/
for dir in */; do
    # Check if it's a directory and contains run_script.sh
    if [ -f "$dir/run_script.sh" ]; then
        echo "Processing $dir"

        # Enter the directory
        cd "$dir" || continue

        # Give execute permissions
        

        # Run the script
        rm -rf gen_results_all gen_results_metrics logs par Par_w_time_th_1.txt Par_w_time.txt

        # Go back to experiments/
        cd ..
    else
        echo "Skipping $dir (no run_script.sh found)"
    fi
done
