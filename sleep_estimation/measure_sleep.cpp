#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

#include <sched.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_ITER 1000000

std::atomic<bool> flag1{false};
std::atomic<bool> flag2{false};

void* my_sleep(void *arg)
{
    flag1.wait(false);
    flag2.store(true);
}

void my_wakeup()
{
    flag1.store(true);
    flag1.notify_one();
}

double seq_loop(double num)
{
    // cout << num << "\n";
    //long seq loop
    double acc = 0;
    double prod = 0;
    for(int i = 0; i<NUM_ITER; ++i)
    {
        prod = num*i;
        acc = acc + prod;
    }

    return acc;
}

int main()
{
    // std::thread th1(my_sleep);
    pthread_t th1;
    pthread_attr_t attr;
    cpu_set_t cpuset;

    pthread_attr_init(&attr);

    CPU_ZERO(&cpuset);
    CPU_SET(6, &cpuset);

    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);

    int result = pthread_create(&th1, &attr, my_sleep, nullptr);
    if (result != 0) 
    {
        std::cerr << "Failed to create thread\n";
        return 1;
    }


    // simulating work
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    seq_loop(100000);

    auto start_time = std::chrono::steady_clock::now();
    // std::thread th2(my_wakeup);
    my_wakeup();
    while(flag2.load() == false)
    {
        // busy waiting        
    }
    auto end_time = std::chrono::steady_clock::now();

    pthread_join(th1, nullptr);
    // th1.join();
    // th2.join();

    pthread_attr_destroy(&attr);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    auto dur_c = duration.count();
    // std::cout << "Time spent to wakeup is " << dur_c << "\n";
    // std::cout << "Estimate of sleep is " << dur_c*2 << "\n";
    std::cout << dur_c*2 << "\n";

    return 0;

}

/*

[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 498575 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 517583 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 490670 nanoseconds


-- performing wakeup as part of main thread to avoid thread init costs
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 70809 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 72765 nanoseconds


-- Seq loop to simulate work instead of dummy sleep
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 59969 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 55188 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 57575 nanoseconds



-- setting thread affinity for pthread associated with my_sleep function
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 40202 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 46634 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 43472 nanoseconds
[dd3888@crunchy5 test_sleep_estimate]$ ./run_script_measure_sleep.sh 
The average of the sleep estimate is 44991 nanoseconds


*/
