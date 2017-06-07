/**
 * Test: Checks the correctness of the library when used by multiple threads and
 * when calling only 'begin()' instead of the 'begin()'/'end()' pair and when
 * using already existing nanomsg sockets and 1 thread only.
 */
#include "../src/knarr.hpp"

#include <stdio.h>
#include <unistd.h>
#include <omp.h>


#define CHNAME "ipc:///tmp/demo.ipc"

#define ITERATIONS 10000
#ifndef TOLERANCE
#define TOLERANCE 0.1 // Between 0 and 1
#endif
#define CUSTOM_VALUE_0 2
#define CUSTOM_VALUE_1 5

// In microseconds
#define LATENCY 3000
#define MONITORING_INTERVAL 1000000


int main(int argc, char** argv){
    if(argc < 2){
        std::cerr << "Usage: " << argv[0] << " [0(Monitor) or 1(Application)]" << std::endl;
        return -1;
    }
    if(atoi(argv[1]) == 0){
        nn::socket socket(AF_SP, NN_PAIR);
        uint chid = socket.bind(CHNAME);

        knarr::Monitor mon(socket, chid);
        //std::cout << "[[Monitor]]: Waiting application start." << std::endl;
        mon.waitStart();
        //std::cout << "[[Monitor]]: Application started." << std::endl;
        knarr::ApplicationSample sample;
        usleep(MONITORING_INTERVAL);
        while(mon.getSample(sample)){
            std::cout << "Received sample: " << sample << std::endl;

            double expectedLatency = LATENCY*1000; // To nanoseconds
            double expectedUtilization = ((double)LATENCY / ((double) (LATENCY))) * 100;
            double expectedTasks = (MONITORING_INTERVAL / ((double)(LATENCY)));
            if(expectedTasks < 1){expectedTasks = 1;}
            if(abs(expectedLatency - sample.latency)/(double) expectedLatency > TOLERANCE){
                std::cerr << "Expected latency: " << expectedLatency <<
                             " Actual latency: " << sample.latency << std::endl;
                return -1;
            }
            if(abs(expectedUtilization - sample.loadPercentage)/(double) expectedUtilization > TOLERANCE){
                std::cerr << "Expected utilization: " << expectedUtilization <<
                             " Actual utilization: " << sample.loadPercentage << std::endl;
                return -1;
            }
            if(abs(expectedTasks - sample.tasksCount)/(double) expectedTasks > TOLERANCE){
                std::cerr << "Expected tasks: " << expectedTasks <<
                             " Actual tasks: " << sample.tasksCount << std::endl;
                return -1;
            }
            usleep(MONITORING_INTERVAL);
        }
        uint expectedExecutionTime = (ITERATIONS*LATENCY) / 1000; // Microseconds to milliseconds
        if(abs(expectedExecutionTime - mon.getExecutionTime())/expectedExecutionTime > TOLERANCE){
            std::cerr << "Expected execution time: " << expectedExecutionTime <<
                         " Actual execution time: " << mon.getExecutionTime() << std::endl;
            return -1;
        }
    }else{
        // Application. Use omp just to test the correctness when multiple
        // threads call begin/end.
        nn::socket socket(AF_SP, NN_PAIR);
        uint chid = socket.connect(CHNAME);

        knarr::Application app(socket, chid);
        for(size_t i = 0; i < ITERATIONS; i++){
            app.begin();
            usleep(LATENCY);
        }
        app.terminate();
    }
    return 0;
}
