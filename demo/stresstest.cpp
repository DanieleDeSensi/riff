/**
 * This is a stresstest to check how many begin()/end() calls we can perform per time unit.
 **/
#include "../src/knarr.hpp"

#include <stdio.h>
#include <unistd.h>
#include <omp.h>

#define CHNAME "ipc:///tmp/demo.ipc"

#define ITERATIONS 100000000
#define NUM_THREADS 2

int main(int argc, char** argv){
    omp_set_num_threads(NUM_THREADS);
    knarr::Application app(CHNAME, NUM_THREADS);
    usleep(5000000);
#pragma omp parallel for
    for(size_t i = 0; i < ITERATIONS; i++){
        int threadId = omp_get_thread_num();
        app.begin(threadId);
        app.end(threadId);
    }
    app.terminate();
    std::cout << "Maximum throughput (iterations/sec): " << app.getTotalTasks()/(app.getExecutionTime()/1000.0) << std::endl;
    return 0;
}
