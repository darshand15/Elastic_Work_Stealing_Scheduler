# Master's Independent Study Research

## Experiments and Investigation of an Elastic Work Stealing Scheduler for Energy Savings

### Advisor: [Prof. Samuel Westrick](https://cs.nyu.edu/~shw8119/)

### Introduction

This Research Project under the mentorship of Prof. Samuel Westrick set out to answer the question: *“In a modern high-performance work-stealing scheduler, when a processor is idle, how long is it typically idle for?”* The insights gleaned from our attempts to answer this question would help in deducing the possible energy savings for a real-world application.

### Motivation

A typical work-stealing scheduler classifies each processor as either working or idle. When idle, a processor randomly attempts to steal from other processors until it finds a task to work on. Hence, idle processors aren’t free; they consume energy to perform random steal attempts. The fundamental idea behind Elastic Scheduling would be to put some idle processors to sleep, if possible, to save energy. However, there exists a significant challenge in deducing how to do so without negatively impacting the end-to-end runtime. The risk with putting a processor to sleep is that the sleeping processor will not notice if new tasks become available. Therefore, it is imperative that the processors are woken back up at just the right moments to ensure that the tasks can be distributed across the awake processors in order to achieve high scalability. However, putting a processor to sleep and waking it back up can be considerably expensive. We can’t possibly hope to convert moments of idleness into energy savings if those moments are really short.

### Implementation Details

* The C++ [parlaylib](https://github.com/cmuparlay/parlaylib/tree/master) library was used for performing all our experiments and investigations of a high-performance elastic work-stealing scheduler. As part of this investigation, various modifications were made to ```scheduler.h``` (contained in ```./include/parlay/```) using different concepts as follows:
    * A worker thread can be said to be in one of 3 different states, namely, Working, Stealing and Sleeping.
    * The transitions between these 3 states can be summarized as follows:
    Working <=> Stealing <=> Sleeping
    * Working state is the initial state for worker 0.
    * Stealing state is the initial state for all workers except worker 0.
    * The general behaviour for these workers is to transition to the stealing state when they have no jobs on their local queue.
    * Under the stealing state, the workers might perform multiple unsuccesful steals, interleaved with sleeping before there is a successful steal attempt. The experiments performed as part of this research project intend to comprehensively identify this non-working duration (unsuccessful steals interleaved with sleep before a successful steal attempt) and explore if they can instead be modified to durations where the worker/processor is put to sleep to maximize energy savings.
    * The different states and transitions have been encoded into the scheduler and continuously tracked.
    * The instrumentation code for timing has been embedded into the scheduler to track these transitions, record appropriate timestamps, and generate logs.
    * The recorded measurements are accumulated appropriately to generate various performance metrics.

* The C++ [spdlog](https://github.com/gabime/spdlog/tree/v1.x) library was used for fast logging in a parallel environment.
* Various experiments contained under ```./experiments``` were created and performed for analysing the behaviour of different parallel benchmarks as follows:
    * 1_pardo_seq_loop: Experiments with a single pardo (the fork-join interface in parlaylib) with two long sequential loops.
    * 2_pardo_seq_loop: Experiments with a single pardo with two long sequential loops having dependencies before and after the pardo.
    * 3_dummy_init_pardo: Experiments with a dummy init pardo to invoke the scheduler and trigger the instrumentation for timing measurement. This is followed by a single pardo with two long sequential loops having dependencies before and after the pardo.
    * 4_nested_pardo: Experiments with nested pardo.
    * 5_par_mergesort: Experiments with parallel mergesort.
    * 6_bigint: Experiments with parallel addition and subtraction of two arbitrary precision numbers.
    * 7_primes: Experiments with parallel generation of primes upto n.
    * 8_bfs: Experiments with parallel bfs traversal of a graph.
    * 9_triangle_count: Experiments with parallel triangle counting of a graph.
    * 10_for_large_iter__parfor_med_iter: Experiments with an outer sequential for loop nested with a parallel for inner loop.
    * 11_parfor_med_iter__for_large_iter: Experiments with a parallel for outer loop nested with a sequential inner loop.
    * 12_delaunay: Experiments with delaunay triangulation in 2 dimensions.
    * 13_mergesort_seq_merge: Experiments with parallel mergesort but with sequential merge.
* Further, experiments contained under ```./sleep_estimation``` were performed to estimate the duration of putting a processor to sleep and waking it up.

### Running the Experiments

* Each sub-directory under ```./experiments``` corresponds to an experiment which is further organized as follows:
    * ```src```: This sub-directory contains the source code for the experiment under consideration
    * ```graph_generation```: This sub-directory contains the code to plot the graphs based on the generated results and logs
    * ```run_script.sh```: This is the script that executes the experiment for different number of threads (1,2,4,8,16,32), generates the corresponding results and logs, and generates the graphs from the logs and results.
* Therefore, all the results and graphs for an experiment can be generated by just running the script, ```./run_script.sh``` under a particular sub-directory of the *experiments* directory.
* The ```./sleep_estimation``` directory contains the experiments to estimate the duration of putting a processor to sleep and waking it up. These experiments can be executed by running the script, ```./run_script_measure_sleep.sh``` under the *sleep_estimation* directory.

### Details regarding the generated Graphs

The graphs generated (*Metrics_Plot* and *Prefix_Sum_Plot*) for each of the experiments have been placed under the *graph_generation* sub-directory of every *experiment's* directory.

**Metrics_Plot.png**

This plot summarizes various performance metrics and contains 4 sub-plots as follows:

1. Number of threads vs Utilization Ratio: This sub-plot contains the number of threads on the x-axis and the Utilization Ratio on the y-axis. The Utilization Ratio is computed as the ratio of (Working Time) to (Working Time + Stealing Time + Sleeping Time).
2. Number of threads vs Successful Steal Ratio: This sub-plot contains the number of threads on the x-axis and the Successful Steal Ratio on the y-axis. The Successful Steal Ratio is computed as the ratio of (number of Successful Steals) to (Total number of Steal Attempts).
3. Number of threads vs Burn Ratio: This sub-plot contains the number of threads on the x-axis and the Burn Ratio on the y-axis. The Burn Ratio is computed as the ratio of (Working Time + Stealing Time) to (Working Time).
4. Box plot for Steal Times (Min, Max, Avg) per Thread: This sub-plot contains the number of threads on the x-axis and the Steal Times on the y-axis. Each box plot represents the Minimum, Maximum, and Average of the Steal Times for a particular execution of the experiment with 'x' number of threads.

**Prefix_Sum_Plot.png**

* This plot summarizes the prefix sums of the Stop-Start Work duration for different executions of the experiment with different number of threads (1,2,4,8,16,32).
* The Stop-Start Work duration corresponds to the difference between the timestamp of the start/beginning of the *(i+1)*<sup>th</sup> working phase and the timestamp of the stop/end of the *i*<sup>th</sup> working phase.
* The x-axis represents the above Stop-Start Work durations in nanoseconds in sorted order
* The y-axis represents an accumulation of Stop-Start Work duration until a particular point (similar to the prefix sum concept which is the reason for the name of the plot). Infact, the prefix sum is normalized by dividing by the total working time of the experiment using 1 thread.
* Therefore, a particular point in the graph can be interpreted as showing the prefix sum of the Stop-Start Work durations until and including the duration denoted by the x co-ordinate for this point under consideration
* The rationale for generating a prefix sum plot was to clearly depict and visualize where the actual Stop-Start Work durations are recorded. Given that the total number of such Stop-Start Work durations can be significantly high and their range can be considerably large, it would be hard to effectively visualize these Stop-Start Work durations if it weren't for a prefix sum plot.
* The reason for normalizing the prefix sum values by dividing by the total working time of the experiment using 1 thread was to be able to comment on the contribution of the non-working time in relation to the working time for 1 thread. As the end of the prefix sum plot corresponds to the sum of all the Stop-Start Work durations, the normalized value of this point denotes the contribution of the non-working time in relation to the working time for the experiment. 
* The red dotted line in each of the sub-plots denotes the estimated sleep duration (gathered from *sleep_estimation* experiments).
* The feasibility percentage noted above each sub-plot is computed as follows: ((summation of the Stop-Start Work durations that exceed the estimated sleep duration) - (estimated sleep duration))/(summation of all the Stop-Start Work durations).
* Therefore, the feasibility percentage denotes what portion of the non-working time can be effectively utilized for energy savings by putting the workers (processor threads) to sleep given that *estimated sleep duration* is an overhead to put a particular worker to sleep.
* Therefore, the feasibility percentage coupled with the normalized end point of the prefix sum plot can help in commenting on the viability and quantity of possible energy savings for a particular experiment.


### Note:
It can be noted that as part of this research project, a race condition in the implementation of the parlaylib scheduler was encountered, the details of which have been recorded as part of the following Github issue: https://github.com/cmuparlay/parlaylib/issues/83