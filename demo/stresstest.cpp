/**
 * This is a stresstest to check how many begin()/end() calls we can perform per time unit
 * and what is the cost of a begin()/end() calls pair.
 **/
#include <riff/riff.hpp>

#include <stdio.h>
#include <unistd.h>
#include <omp.h>
#include <cmath>

#define CHNAME "ipc:///tmp/demo.ipc"

#define ITERATIONS 1000000000
#define NUM_THREADS 2
#define STARTX 16031.099125085183

int main(int argc, char** argv){
    omp_set_num_threads(NUM_THREADS);
    riff::Application app(CHNAME, NUM_THREADS);
    usleep(5000000);
    double x = STARTX;
    ulong start = riff::getCurrentTimeNs();
#pragma omp parallel for
    for(size_t i = 0; i < ITERATIONS; i++){
        int threadId = omp_get_thread_num();
        app.begin(threadId);
        x = std::sin(x);
        app.end(threadId);
    }
    ulong instrumentedDuration = riff::getCurrentTimeNs() - start;
    app.terminate();
    std::cout << "dummy1: " << x << std::endl; // Needed to avoid compiler optimizations which could remove processing of variable x.
    std::cout << "Maximum throughput (iterations/sec): " << app.getTotalTasks()/(app.getExecutionTime()/1000.0) << std::endl;

    ulong nonInstrumentedDuration = 0;
    do{
        x = STARTX;
        start = riff::getCurrentTimeNs();
#pragma omp parallel for
        for(size_t i = 0; i < ITERATIONS; i++){
            x = std::sin(x);;
        }
        nonInstrumentedDuration = riff::getCurrentTimeNs() - start;
    }while(nonInstrumentedDuration > instrumentedDuration);

    std::cout << "dummy2: " << x << std::endl; // Needed to avoid compiler optimizations which could remove processing of variable x.
    std::cout << "begin-end pair overhead (ms): " << ((instrumentedDuration - nonInstrumentedDuration) / (double) ITERATIONS) / 1000000.0 << std::endl;
    return 0;
}
