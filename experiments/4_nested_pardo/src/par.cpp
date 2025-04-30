#include <iostream>
#include <cstdlib>
#include <chrono>
#include <parlay/parallel.h>
#include <sys/time.h>
#include <sys/resource.h>

#define NUM_ITER 1000000000

using namespace std;

double seq_loop(double num)
{
    double acc = 0;
    double prod = 0;
    for(int i = 0; i<NUM_ITER; ++i)
    {
        prod = num*i;
        acc = acc + prod;
    }

    return acc;
}

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        cout << "Enter command line args\n";
    }

    // struct rusage usage;
    // struct timeval start_utime, end_utime;
    // struct timeval start_stime, end_stime;
    // double duration_utime, duration_stime;

    // getrusage(RUSAGE_SELF, &usage);
    // start_utime = usage.ru_utime;
    // start_stime = usage.ru_stime;

    auto start = std::chrono::high_resolution_clock::now();

    double ans1 = seq_loop(atoi(argv[1]));
    double ans2 = 0;
    double ans3 = 0;

    parlay::par_do(
      [&]() 
      { 
        double ans_temp = seq_loop(ans1); 
        parlay::par_do(
            [&]() { ans2 = seq_loop(ans1 + ans_temp); },
            [&]() { ans3 = seq_loop(ans1 + ans_temp + atoi(argv[1])); }
        );
        ans2 = ans2 + ans3;
      },
      [&]() { ans3 = seq_loop(ans1 + atoi(argv[1])); }
    );

    double ans4 = seq_loop(ans2 + ans3);

    auto stop = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
    cout << "Parallel End to End Wall Clock Time Taken: " << duration.count() << " nanoseconds\n";


    // getrusage(RUSAGE_SELF, &usage);
    // end_utime = usage.ru_utime;
    // end_stime = usage.ru_stime;

    // duration_utime = (end_utime.tv_sec - start_utime.tv_sec)*1000000 + (end_utime.tv_usec - start_utime.tv_usec);
    // duration_stime = (end_stime.tv_sec - start_stime.tv_sec)*1000000 + (end_stime.tv_usec - start_stime.tv_usec);
    
    // cout << "\n===========================================================================\n";
    // cout << "\n\nParallel User Time Taken: " << duration_utime << " microseconds\n";
    // cout << "Parallel System Time Taken: " << duration_stime << " microseconds\n";

    return 0;
}