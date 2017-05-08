#include "../src/knarr.hpp"

#include <stdio.h>
#include <unistd.h>

#define CHNAME "ipc:///tmp/demo.ipc"

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
        while(true){
            sleep(1);
            mon.getSample(sample);
            std::cout << "Received sample: " << sample << std::endl;
        }
    }else{
        knarr::Application app(CHNAME);
        std::cout << "[[Application]] Created." << std::endl;
        sleep(5);
        while(true){
            std::cout << "[[Application]] Receiving." << std::endl;
            sleep(1);
            std::cout << "[[Application]] Computing." << std::endl;
            app.begin();
            sleep(3);
            std::cout << "[[Application]] Computed." << std::endl;
            app.end();
        }
        app.terminate();
    }
    return 0;
}
