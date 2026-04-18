# import os
# import matplotlib.pyplot as plt
# import sys

# def read_logs_from_folder(folder_path, plot_name):
#     fig, axs = plt.subplots(3, 2, figsize=(23, 7))
#     axs = axs.flatten()
#     i = 0
#     norm_factor = 1
#     num_threads = [1,2,4,8,16,32]
#     with open("../Par_w_time_th_1.txt", "r") as norm_file:
#         lines = norm_file.readlines()
#         line = lines[0]
#         line = line.strip()
#         norm_factor = (int)(line)

#     w = []
#     with open("../Par_w_time.txt", "r") as w_file:
#         lines = w_file.readlines()
#         for line in lines:
#             line = line.strip()
#             w.append((int)(line))

#     for filename in sorted(os.listdir(folder_path), key=lambda x: int(x.split('_')[-1].split('.')[0])):
#         file_path = os.path.join(folder_path, filename)
#         print(file_path)
#         arr_data = []
#         c_gt_sleep_dur = 0
#         fi = 0

#         if os.path.isfile(file_path) and filename.lower().endswith((".log", ".txt")):
                
#             with open(file_path, 'r') as file:
                
#                 for line in file:
#                     line = line.strip()
#                     split_l = line.split()
#                     en = (int)(split_l[-1])
#                     st = (int)(split_l[-2])
#                     diff = en - st
#                     arr_data.append(diff)

#                     if diff > sleep_estimate:
#                         c_gt_sleep_dur += diff
#                         fi += (diff - sleep_estimate)


#             ratio_pow_sav_feas = 0
#             ratio_pot_energy_sav = 0
#             data_x = sorted(arr_data)
#             prefix_sum_data_y = []
            
#             if len(arr_data) != 0:
#                 ratio_pow_sav_feas = round(((c_gt_sleep_dur - sleep_estimate)/sum(arr_data))*100, 2)
#                 ti = sum(arr_data)
#                 wi = w[i]
#                 ratio_pot_energy_sav = (wi + ti)/(wi + ti - fi)
#                 ratio_pot_energy_sav = round(ratio_pot_energy_sav, 2)

#                 prefix_sum_data_y.append(data_x[0]/norm_factor)
#                 for ind in range(1, len(data_x)):
#                     prefix_sum_data_y.append(prefix_sum_data_y[ind-1] + (data_x[ind]/norm_factor))
                    
            
#             if ratio_pow_sav_feas < 0:
#                 ratio_pow_sav_feas = 0

#             if len(data_x) < 100:
#                 axs[i].plot(data_x, prefix_sum_data_y, marker='o')
#             else:
#                 axs[i].plot(data_x, prefix_sum_data_y)
#                 axs[i].set_xscale('log')
                
#             axs[i].set_xlabel('Duration (in ns)')
#             axs[i].set_ylabel('Prefix_Sum(duration)')
#             axs[i].axvline(x=sleep_estimate, color='r', linestyle='--', label=f'x={sleep_estimate}')
#             # axs[i].set_title(str(num_threads[i]) + " Thread(s): " + str(ratio_pow_sav_feas) + "%" + " feasibility")
#             axs[i].set_title(str(num_threads[i]) + " Thread(s): " + str(ratio_pot_energy_sav) + " feasibility")


#             plt.suptitle('Prefix Sum Plot with Feasibility metric for Stop-Start Work Duration')                

#         i += 1
    
#     plt.tight_layout()
#     plt.savefig(plot_name)


# if len(sys.argv) > 1:
#     sleep_estimate = (int)(sys.argv[1])

# folder_path = "../logs/"
# read_logs_from_folder(folder_path, "./Prefix_Sum_Plot.png")





import os
import matplotlib.pyplot as plt
import sys

def read_logs_from_folder(folder_path, plot_name, sleep_estimate):
    # Create a single plot canvas
    fig, ax = plt.subplots(figsize=(12, 8))
    
    norm_factor = 1
    num_threads = [1, 2, 4, 8, 16, 32]
    
    # Define distinct colors (excluding red) and distinct shapes/linestyles
    colors = ['blue', 'green', 'darkorange', 'purple', 'cyan', 'saddlebrown', 'magenta', 'olive', 'teal', 'navy']
    markers = ['o', 's', '^', 'D', 'v', 'P', '*', 'X', '<', '>']
    linestyles = ['-', '--', '-.', ':', '-', '--', '-.', ':', '-', '--']
    
    # Read norm factor
    with open("../Par_w_time_th_1.txt", "r") as norm_file:
        lines = norm_file.readlines()
        line = lines[0].strip()
        norm_factor = int(line)

    # Read w array
    w = []
    with open("../Par_w_time.txt", "r") as w_file:
        lines = w_file.readlines()
        for line in lines:
            line = line.strip()
            w.append(int(line))

    i = 0
    
    # Loop through all files and plot them on the same axis
    for filename in sorted(os.listdir(folder_path), key=lambda x: int(x.split('_')[-1].split('.')[0])):
        file_path = os.path.join(folder_path, filename)
        print(file_path)
        arr_data = []
        c_gt_sleep_dur = 0
        fi = 0

        if os.path.isfile(file_path) and filename.lower().endswith((".log", ".txt")):
                
            with open(file_path, 'r') as file:
                for line in file:
                    line = line.strip()
                    split_l = line.split()
                    en = int(split_l[-1])
                    st = int(split_l[-2])
                    diff = en - st
                    arr_data.append(diff)

                    if diff > sleep_estimate:
                        c_gt_sleep_dur += diff
                        fi += (diff - sleep_estimate)

            ratio_pow_sav_feas = 0
            ratio_pot_energy_sav = 0
            data_x = sorted(arr_data)
            prefix_sum_data_y = []
            
            if len(arr_data) != 0:
                ratio_pow_sav_feas = round(((c_gt_sleep_dur - sleep_estimate)/sum(arr_data))*100, 2)
                ti = sum(arr_data)
                
                # Prevent index out of bounds if there are more files than entries in w
                wi = w[i] if i < len(w) else 0 
                
                # Prevent division by zero
                denominator = (wi + ti - fi)
                if denominator != 0:
                    ratio_pot_energy_sav = (wi + ti) / denominator
                ratio_pot_energy_sav = round(ratio_pot_energy_sav, 2)

                prefix_sum_data_y.append(data_x[0] / norm_factor)
                for ind in range(1, len(data_x)):
                    prefix_sum_data_y.append(prefix_sum_data_y[ind-1] + (data_x[ind] / norm_factor))
                    
            if ratio_pow_sav_feas < 0:
                ratio_pow_sav_feas = 0

            # Construct the label for the legend
            thread_label = num_threads[i] if i < len(num_threads) else f"File {i}"
            plot_label = f"{thread_label} Thread(s): {ratio_pot_energy_sav} feasibility"

            # Select distinct styling for the current line
            c = colors[i % len(colors)]
            m = markers[i % len(markers)]
            ls = linestyles[i % len(linestyles)]

            # Plot on the single axis (ax) with unique shape and color
            if len(data_x) < 100:
                ax.plot(data_x, prefix_sum_data_y, color=c, marker=m, linestyle=ls, markersize=5, alpha=0.8, label=plot_label)
            else:
                # Using a float for markevery (e.g., 0.2) places exactly 5 markers evenly spaced by *visual physical distance*
                # rather than index. This prevents heavy cluttering and bunching at the ends of log-scaled axes.
                ax.plot(data_x, prefix_sum_data_y, color=c, marker=m, linestyle=ls, markersize=5, alpha=0.8, markevery=0.2, label=plot_label)
                ax.set_xscale('log')
                
        i += 1
    
    # Configure global plot settings
    ax.set_xlabel('Idle Duration (in ns)')
    ax.set_ylabel('Work-Normalized Prefix_Sum of Idle Durations')
    
    # Red is strictly reserved for the vertical line here
    ax.axvline(x=sleep_estimate, color='red', linestyle='--', linewidth=2, label=f'Sleep Estimate = {round(sleep_estimate/1000, 2)} µs')
    
    # Add a legend so we know which line represents which thread count
    ax.legend(loc='best')
    
    plt.tight_layout()
    plt.savefig(plot_name)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        sleep_estimate = int(sys.argv[1])
    else:
        print("Usage: python unified_plot.py <sleep_estimate_in_ns>")
        sys.exit(1)

    folder_path = "../logs/"
    read_logs_from_folder(folder_path, "./Prefix_Sum_Plot_Unified.png", sleep_estimate)