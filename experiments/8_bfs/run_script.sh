#!/bin/bash

mkdir ./logs
mkdir ./gen_results_all
mkdir ./gen_results_metrics

export PARLAY_NUM_THREADS=1
g++ -std=c++17 -I../../include/ -pthread ./src/BFS.cpp -o par
./par 1000000
mv ./logs/w_pair.txt ./logs/w_pair_th_1.txt
mv Results_all.txt ./gen_results_all/gen_results_all_th_1.txt
mv Results_metrics.txt ./gen_results_metrics/gen_results_metrics_th_1.txt
mv Par_w_time.txt Par_w_time_th_1.txt


export PARLAY_NUM_THREADS=2
g++ -std=c++17 -I../../include/ -pthread ./src/BFS.cpp -o par
./par 1000000
mv ./logs/w_pair.txt ./logs/w_pair_th_2.txt
mv Results_all.txt ./gen_results_all/gen_results_all_th_2.txt
mv Results_metrics.txt ./gen_results_metrics/gen_results_metrics_th_2.txt


export PARLAY_NUM_THREADS=4
g++ -std=c++17 -I../../include/ -pthread ./src/BFS.cpp -o par
./par 1000000
mv ./logs/w_pair.txt ./logs/w_pair_th_4.txt
mv Results_all.txt ./gen_results_all/gen_results_all_th_4.txt
mv Results_metrics.txt ./gen_results_metrics/gen_results_metrics_th_4.txt


export PARLAY_NUM_THREADS=8
g++ -std=c++17 -I../../include/ -pthread ./src/BFS.cpp -o par
./par 1000000
mv ./logs/w_pair.txt ./logs/w_pair_th_8.txt
mv Results_all.txt ./gen_results_all/gen_results_all_th_8.txt
mv Results_metrics.txt ./gen_results_metrics/gen_results_metrics_th_8.txt


export PARLAY_NUM_THREADS=16
g++ -std=c++17 -I../../include/ -pthread ./src/BFS.cpp -o par
./par 1000000
mv ./logs/w_pair.txt ./logs/w_pair_th_16.txt
mv Results_all.txt ./gen_results_all/gen_results_all_th_16.txt
mv Results_metrics.txt ./gen_results_metrics/gen_results_metrics_th_16.txt


export PARLAY_NUM_THREADS=32
g++ -std=c++17 -I../../include/ -pthread ./src/BFS.cpp -o par
./par 1000000
mv ./logs/w_pair.txt ./logs/w_pair_th_32.txt
mv Results_all.txt ./gen_results_all/gen_results_all_th_32.txt
mv Results_metrics.txt ./gen_results_metrics/gen_results_metrics_th_32.txt

rm Par_w_time.txt
cd graph_generation
python3 ./generate_graph_ratio_metrics.py
python3 ./generate_graph_prefix_sum_logs.py 44991
