#include "../src/knarr.hpp"

#include <stdio.h>
#include <unistd.h>
#include <omp.h>


#define CHNAME "ipc:///tmp/demo.ipc"

#define ITERATIONS 10000
#define NUM_THREADS 2
#define CUSTOM_VALUE_0 2
#define CUSTOM_VALUE_1 5

// In microseconds
#define IDLE_TIME 1000
#define LATENCY 3000
#define MONITORING_INTERVAL 1000000

class DemoAggregator: public knarr::Aggregator{
public:
    double aggregate(size_t index, const std::vector<double>& customValues){
        double r = 0;
        for(double d : customValues){
            r += d;
        }
        return r;
    }
};

int main(int argc, char** argv){
    if(argc < 2){
        std::cerr << "Usage: " << argv[0] << " [0(Monitor) or 1(Application)]" << std::endl;
        return -1;
    }
    if(atoi(argv[1]) == 0){
        knarr::Monitor mon(CHNAME);
        std::cout << "[[Monitor]]: Waiting application start." << std::endl;
        mon.waitStart();
        std::cout << "[[Monitor]]: Application started." << std::endl;
        knarr::ApplicationSample sample;
        usleep(MONITORING_INTERVAL);
        while(mon.getSample(sample)){
            std::cout << "Received sample: " << sample << std::endl;
            usleep(MONITORING_INTERVAL);
        }
    }else{
        omp_set_num_threads(NUM_THREADS);
        knarr::Application app(CHNAME, NUM_THREADS, new DemoAggregator());
        usleep(5000000);
#pragma omp parallel for
        for(size_t i = 0; i < ITERATIONS; i++){
            int threadId = omp_get_thread_num();
            std::cout << "[[Application]] Receiving." << std::endl;
            // Simulates the overhead of the data scheduling/receiving
            usleep(IDLE_TIME);
            std::cout << "[[Application]] Computing." << std::endl;
            app.begin(threadId);
            // Simulates the computation latency
            usleep(LATENCY);
            std::cout << "[[Application]] Computed." << std::endl;
            app.storeCustomValue(0, CUSTOM_VALUE_0, threadId);
            app.storeCustomValue(1, CUSTOM_VALUE_1, threadId);
            app.end(threadId);
        }
        app.terminate();
    }
    return 0;
}
