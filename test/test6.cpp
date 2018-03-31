/**
 * Test: Checks the correctness of the library
 * using already existing nanomsg sockets and 1 thread only and checking
 * correctness of markInconsistentSamples call.
 */
#include "../src/riff.hpp"

#include <stdio.h>
#include <unistd.h>
#include <omp.h>


#define CHNAME "ipc:///tmp/demo.ipc"

#define ITERATIONS 5000
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

        riff::Monitor mon(socket, chid);
        //std::cout << "[[Monitor]]: Waiting application start." << std::endl;
        mon.waitStart();
        //std::cout << "[[Monitor]]: Application started." << std::endl;
        riff::ApplicationSample sample;
        usleep(MONITORING_INTERVAL);
        while(mon.getSample(sample)){
            std::cout << "Received sample: " << sample << std::endl;
            std::cout << "PhaseId: " << mon.getPhaseId() << std::endl;
            std::cout << "Total threads: " << mon.getTotalThreads() << std::endl;

            assert(sample.inconsistent);
            usleep(MONITORING_INTERVAL);
        }
    }else{
        nn::socket socket(AF_SP, NN_PAIR);
        uint chid = socket.connect(CHNAME);

        riff::Application app(socket, chid);
        app.markInconsistentSamples();
        for(size_t i = 0; i < ITERATIONS; i++){
            app.begin();
            usleep(LATENCY);
            app.setPhaseId(i);
            app.end();
        }
        app.terminate();
    }
    return 0;
}
