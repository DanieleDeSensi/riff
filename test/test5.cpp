/**
 * Test: Checks the correctness of the library when used by multiple threads.
 */
#include <riff/riff.hpp>

#include <stdio.h>
#include <unistd.h>
#include <omp.h>


#define CHNAME "ipc:///tmp/demo.ipc"

#define ITERATIONS 10000
#define NUM_THREADS 2
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
        riff::Monitor mon(CHNAME);
        std::cout << "[[Monitor]]: Waiting application start." << std::endl;
        mon.waitStart();
        std::cout << "[[Monitor]]: Application started." << std::endl;
        riff::ApplicationSample sample;
        usleep(MONITORING_INTERVAL);
        while(mon.getSample(sample)){
            std::cout << "Received sample: " << sample << std::endl;
            usleep(MONITORING_INTERVAL);
        }
        std::cout << "Execution time: " << mon.getExecutionTime() << std::endl;
        std::cout << "Total tasks: " <<  mon.getTotalTasks() << std::endl;
    }else{
        // Application. Use omp just to test the correctness when multiple
        // threads call begin/end.
        omp_set_num_threads(NUM_THREADS);
        riff::Application app(CHNAME, NUM_THREADS);
        riff::ApplicationConfiguration conf;
        app.setConfiguration(conf);
#pragma omp parallel for schedule(dynamic, 1)
        for(size_t i = 0; i < ITERATIONS; i++){
            bool exc1 = false, exc2 = false;
            int threadId = omp_get_thread_num();
            app.begin(threadId);
            if(i == 0){
                // Check if calling begin twice in a row throws an exception
                try{
                    app.begin(threadId);
                }catch(const std::exception& e){
                    exc1 = true;
                }
                if(!exc1){
                    throw std::runtime_error("Exception expected\n");
                }
            }
            if(threadId == 0){
                // Simulates a very slow thread
                sleep(2);
            }else{
                usleep(rand() % LATENCY);
            }
            app.end(threadId);

            // Check errors on wrong parameters
            try{
                // Wrong field id
                app.storeCustomValue(RIFF_MAX_CUSTOM_FIELDS + 1, 0, threadId);
            }catch(const std::exception& e){
                exc1 = true;
            }

            try{
                // Wrong thread id
                app.storeCustomValue(RIFF_MAX_CUSTOM_FIELDS - 1, 0, 99999);
            }catch(const std::exception& e){
                exc2 = true;
            }

            if(!(exc1 && exc2)){
                throw std::runtime_error("Exception expected\n");
            }
        }
        app.terminate();
        std::cout << "Execution time: " << app.getExecutionTime() << std::endl;
        std::cout << "Total tasks: " <<  app.getTotalTasks() << std::endl;
    }
    return 0;
}
