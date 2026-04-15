#!/bin/bash

# Loop over all items inside experiments/
for dir in */; do
    # Check if it's a directory and contains run_script.sh
    if [ -f "$dir/run_script.sh" ]; then
        echo "Processing $dir"

        # Enter the directory
        cd "$dir" || continue

        # Give execute permissions
        chmod 777 run_script.sh

        # Run the script
        ./run_script.sh

        # Go back to experiments/
        cd ..
    else
        echo "Skipping $dir (no run_script.sh found)"
    fi
done