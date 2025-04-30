import matplotlib.pyplot as plt
import numpy as np
import os

num_threads = []
utilization_ratio = []
successful_steal_ratio = []
burn_ratio = []
min_steal_time = []
max_steal_time = []
avg_steal_time = []


for filename in sorted(os.listdir("../gen_results_metrics"), key = lambda f: (int)((f.split('_')[4]).split('.')[0])):
    file_path = os.path.join("../gen_results_metrics", filename)
    if os.path.isfile(file_path) and filename.lower().endswith((".log", ".txt")):
        with open(file_path, 'r') as file:
            for line in file:
                line = line.strip()
                line = line.split(',')
                num_threads.append((int)(line[0]))
                utilization_ratio.append((float)(line[1]))
                successful_steal_ratio.append((float)(line[2]))
                burn_ratio.append((float)(line[3]))
                min_steal_time.append((float)(line[4]))
                max_steal_time.append((float)(line[5]))
                avg_steal_time.append((float)(line[6]))

num_threads = np.array(num_threads)
fig, axs = plt.subplots(2, 2, figsize=(14, 10))

# Plot 1: Number of threads vs Utilization Ratio
axs[0, 0].plot(num_threads, utilization_ratio, marker='o', color='b', label='Utilization Ratio')
axs[0, 0].set_title('Number of threads vs Utilization Ratio')
axs[0, 0].set_xlabel('Number of threads')
axs[0, 0].set_ylabel('Utilization Ratio (W/(W+St+Sl))')
axs[0, 0].grid(True)
axs[0, 0].set_xticks(num_threads)
axs[0, 0].set_xticklabels([str(x) for x in num_threads])

# Plot 2: Number of threads vs Successful Steal Ratio
axs[0, 1].plot(num_threads, successful_steal_ratio, marker='o', color='r', label='Successful Steal Ratio')
axs[0, 1].set_title('Number of threads vs Successful Steal Ratio')
axs[0, 1].set_xlabel('Number of threads')
axs[0, 1].set_ylabel('Successful Steal Ratio (Succ steals/Steal attempts)')
axs[0, 1].grid(True)
axs[0, 1].set_xticks(num_threads)
axs[0, 1].set_xticklabels([str(x) for x in num_threads])

# Plot 3: Number of threads vs Burn Ratio
axs[1, 0].plot(num_threads, burn_ratio, marker='o', color='g', label='Burn Ratio')
axs[1, 0].set_title('Number of threads vs Burn Ratio')
axs[1, 0].set_xlabel('Number of threads')
axs[1, 0].set_ylabel('Burn Ratio ((W+St)/W)')
axs[1, 0].grid(True)
axs[1, 0].set_xticks(num_threads)
axs[1, 0].set_xticklabels([str(x) for x in num_threads])

# Plot 4: Box plot for Min, Max, and Avg Steal Time (per thread)
# Create a list of lists, where each list contains the min, max, and avg values for a particular thread count
data = [ [min_steal_time[i], max_steal_time[i], avg_steal_time[i]] for i in range(len(num_threads)) ]

# Plot boxplot for each thread count
axs[1, 1].boxplot(data, positions=num_threads-1)  # Shift positions to match thread numbers
axs[1, 1].set_title('Box plot for Steal Times (Min, Max, Avg) per Thread')
axs[1, 1].set_xlabel('Number of threads')
axs[1, 1].set_ylabel('Steal Time (ms)')
axs[1, 1].set_xticklabels(num_threads)

plt.tight_layout()
plt.savefig('./Metrics_Plot.png')

