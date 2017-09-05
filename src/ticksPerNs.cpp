/*
 * This file is part of knarr
 *
 * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include <iostream>
#include <sstream>
#include <cmath>
#include <sched.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

//  Windows
#ifdef _WIN32 
#include <intrin.h>
inline uint64_t rdtsc(){
    return __rdtsc();
}
//  Linux/GCC
#else
inline uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}
#endif

unsigned long getNanoSeconds(){
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_sec * 1000000000 + spec.tv_nsec;
}

double getTicksPerNanosec(){
    double x = 0.691812048120;

    unsigned long start = getNanoSeconds();
    uint64_t t1 = rdtsc();
    while(rdtsc() - t1 < 1000000){
        x = std::sin(x);
    }
    uint64_t t2 = rdtsc();
    unsigned long end = getNanoSeconds();

    std::ostringstream sstream;
    sstream << x;
    FILE *p = popen(std::string("echo " + sstream.str() +
                                " >/dev/null").c_str(), "r");
    pclose(p);

    return ((double) (t2 - t1) / (double) (end - start));
}

int main(int argc, char** argv){
#ifndef __linux__
#error "This only works on Linux"
#endif
    // Pin to a core because TSC may not be coherent between different cores.
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    assert(!sched_setaffinity(0, sizeof(mask), &mask));
    setpriority(PRIO_PROCESS, 0, -20);

    uint numIterations = 1000;
    double x = 0;
    for(size_t i = 0; i < numIterations; i++){
        x += getTicksPerNanosec();
    }
    std::cout << x / (double) numIterations << std::endl;
}

