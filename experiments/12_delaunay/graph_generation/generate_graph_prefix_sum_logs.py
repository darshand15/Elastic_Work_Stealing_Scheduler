import os
import matplotlib.pyplot as plt
import sys

def read_logs_from_folder(folder_path, plot_name):
    fig, axs = plt.subplots(3, 2, figsize=(23, 7))
    axs = axs.flatten()
    i = 0
    norm_factor = 1
    num_threads = [1,2,4,8,16,32]
    with open("../Par_w_time_th_1.txt", "r") as norm_file:
        lines = norm_file.readlines()
        line = lines[0]
        line = line.strip()
        norm_factor = (int)(line)

    w = []
    with open("../Par_w_time.txt", "r") as w_file:
        lines = w_file.readlines()
        for line in lines:
            line = line.strip()
            w.append((int)(line))

    for filename in sorted(os.listdir(folder_path)):
        file_path = os.path.join(folder_path, filename)
        arr_data = []
        c_gt_sleep_dur = 0
        fi = 0

        if os.path.isfile(file_path) and filename.lower().endswith((".log", ".txt")):
                
            with open(file_path, 'r') as file:
                
                for line in file:
                    line = line.strip()
                    split_l = line.split()
                    en = (int)(split_l[-1])
                    st = (int)(split_l[-2])
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
                wi = w[i]
                ratio_pot_energy_sav = (wi + ti)/(wi + ti - fi)

                prefix_sum_data_y.append(data_x[0]/norm_factor)
                for ind in range(1, len(data_x)):
                    prefix_sum_data_y.append(prefix_sum_data_y[ind-1] + (data_x[ind]/norm_factor))
                    
            
            if ratio_pow_sav_feas < 0:
                ratio_pow_sav_feas = 0

            if len(data_x) < 100:
                axs[i].plot(data_x, prefix_sum_data_y, marker='o')
            else:
                axs[i].plot(data_x, prefix_sum_data_y)
                
            axs[i].set_xlabel('Duration (in ns)')
            axs[i].set_ylabel('Prefix_Sum(duration)')
            axs[i].axvline(x=sleep_estimate, color='r', linestyle='--', label=f'x={sleep_estimate}')
            # axs[i].set_title(str(num_threads[i]) + " Thread(s): " + str(ratio_pow_sav_feas) + "%" + " feasibility")
            axs[i].set_title(str(num_threads[i]) + " Thread(s): " + str(ratio_pot_energy_sav) + " feasibility")


            plt.suptitle('Prefix Sum Plot with Feasibility metric for Stop-Start Work Duration')                

        i += 1
    
    plt.tight_layout()
    plt.savefig(plot_name)


if len(sys.argv) > 1:
    sleep_estimate = (int)(sys.argv[1])

folder_path = "../logs/"
read_logs_from_folder(folder_path, "./Prefix_Sum_Plot.png")
