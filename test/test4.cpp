/**
 * Test: Checks the correctness of operators on samples.
 */
#include "../src/knarr.hpp"

#include <stdio.h>
#include <unistd.h>
#include <omp.h>
#include <sstream>

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
    knarr::ApplicationSample sample;
    sample.bandwidth = 1;
    sample.latency = 2;
    sample.loadPercentage = 3;
    sample.numTasks = 4;
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        sample.customFields[i] = 4 + i + 1;
    }

    // Assignment
    knarr::ApplicationSample sample2 = sample;

    // Multiplication by constant
    sample2 *= 10;
    assert(sample2.bandwidth == sample.bandwidth*10);
    assert(sample2.latency == sample.latency*10);
    assert(sample2.loadPercentage == sample.loadPercentage*10);
    assert(sample2.numTasks == sample.numTasks*10);
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        assert(sample2.customFields[i] == sample.customFields[i]*10);
    }

    // Division by constant
    sample2 /= 10;
    assert(sample2.bandwidth == sample.bandwidth);
    assert(sample2.latency == sample.latency);
    assert(sample2.loadPercentage == sample.loadPercentage);
    assert(sample2.numTasks == sample.numTasks);
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        assert(sample2.customFields[i] == sample.customFields[i]);
    }

    // Copy constructor and sum
    knarr::ApplicationSample r(sample + sample2);
    assert(r.bandwidth == sample.bandwidth + sample2.bandwidth);
    assert(r.latency == sample.latency + sample2.latency);
    assert(r.loadPercentage == sample.loadPercentage + sample2.loadPercentage);
    assert(r.numTasks == sample.numTasks + sample2.numTasks);
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        assert(r.customFields[i] == sample.customFields[i] + sample2.customFields[i]);
    }

    // Subtraction
    r = sample - sample2;
    assert(r.bandwidth == 0);
    assert(r.latency == 0);
    assert(r.loadPercentage == 0);
    assert(r.numTasks == 0);
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        assert(r.customFields[i] == 0);
    }

    // Multiplication
    r = sample * sample2;
    assert(r.bandwidth == sample.bandwidth * sample2.bandwidth);
    assert(r.latency == sample.latency * sample2.latency);
    assert(r.loadPercentage == sample.loadPercentage * sample2.loadPercentage);
    assert(r.numTasks == sample.numTasks * sample2.numTasks);
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        assert(r.customFields[i] == sample.customFields[i] * sample2.customFields[i]);
    }

    // Division
    r = sample / sample2;
    assert(r.bandwidth == sample.bandwidth / sample2.bandwidth);
    assert(r.latency == sample.latency / sample2.latency);
    assert(r.loadPercentage == sample.loadPercentage / sample2.loadPercentage);
    assert(r.numTasks == sample.numTasks / sample2.numTasks);
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        assert(r.customFields[i] == sample.customFields[i] / sample2.customFields[i]);
    }

    // Load
    std::string fieldStr = "[Load: 90 Bandwidth: 100 Latency: 200 NumTasks: 300 "
                           "CustomField0: 0 CustomField1: 1 CustomField2: 2 "
                           "CustomField3: 3 CustomField4: 4 CustomField5: 5 "
                           "CustomField6: 6 CustomField7: 7 CustomField8: 8 "
                           "CustomField9: 9]";
    std::stringstream ss(fieldStr);
    ss >> sample;
    assert(sample.loadPercentage == 90);
    assert(sample.bandwidth == 100);
    assert(sample.latency == 200);
    assert(sample.numTasks == 300);
    for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
        assert(sample.customFields[i] == i);
    }
    return 0;
}
