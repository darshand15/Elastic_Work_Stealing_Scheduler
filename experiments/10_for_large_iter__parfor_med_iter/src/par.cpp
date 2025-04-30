#include <iostream>
#include <cstdlib>
#include <chrono>
#include <parlay/parallel.h>
#include <sys/time.h>
#include <sys/resource.h>

#define LARGE_ITER 100000
#define MED_ITER 1000
#define SEQ_LOOP_ITER 1

using namespace std;

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();

    for(int i = 0; i<LARGE_ITER; ++i)
    {
        parlay::parallel_for(0, MED_ITER, [&](size_t j){
            double acc = 0;
            double prod = 0;
            for(int k = 0; k<SEQ_LOOP_ITER; ++k)
            {
                prod = (i+j+1)*k;
                acc = acc + prod;
            }

            return acc;
        }, 250);
    }

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
    cout << "Parallel End to End Wall Clock Time Taken: " << duration.count() << " nanoseconds\n";

    return 0;
}