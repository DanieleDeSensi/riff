/*
 * This file is part of knarr
 *
 * (c) 2016- Daniele De Sensi (d.desensi.software@gmail.com)
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#include "external/cppnanomsg/nn.hpp"

#include "knarr.hpp"

#include <cmath>
#include <errno.h>
#include <limits>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#undef DEBUG
#undef DEBUGB

#ifdef DEBUG_KNARR
#define DEBUG(x) do { std::cerr << "[Knarr] " << x << std::endl; } while (0)
#define DEBUGB(x) do {x;} while (0)
#else
#define DEBUG(x)
#define DEBUGB(x)
#endif

namespace knarr{

void* applicationSupportThread(void* data){
    Application* application = static_cast<Application*>(data);

    while(!application->_supportStop){
        Message recvdMsg;
        int res = application->_channelRef.recv(&recvdMsg, sizeof(recvdMsg), 0);
        if(res == sizeof(recvdMsg)){
            assert(recvdMsg.type == MESSAGE_TYPE_SAMPLE_REQ);
            // Prepare response message.
            Message msg;
            msg.type = MESSAGE_TYPE_SAMPLE_RES;
            msg.payload.sample = ApplicationSample(); // Set sample to all zeros

            // Add the samples of all the threads.
            size_t updatedSamples = 0;
            for(size_t i = 0; i < application->_threadData.size(); i++){
                ThreadData& toAdd = application->_threadData[i];
                ApplicationSample sample = toAdd.sample;
                const ApplicationSample& chkSample = toAdd.sample;
                const ThreadData& chkToAdd = application->_threadData[i];
                
                // Wait for thread to store a sample
                // (unless quickReply was set to true).                
                do{                    
                    sched_yield();
                }while((!chkSample.latency || !chkToAdd.idleTime) && 
                       !application->_quickReply && 
                       !application->_supportStop);

                ulong totalTime = (sample.latency + toAdd.idleTime);
                if(totalTime){
                    sample.bandwidth = sample.numTasks / (totalTime / 1000000000.0); // From tasks/ns to tasks/sec
                    sample.loadPercentage = (sample.latency / totalTime) * 100.0;
                    sample.latency /= sample.numTasks;
                    ++updatedSamples;

                    msg.payload.sample.bandwidth += sample.bandwidth;
                    msg.payload.sample.latency += sample.latency;
                    msg.payload.sample.loadPercentage += sample.loadPercentage;
                    msg.payload.sample.numTasks += sample.numTasks;
                }
            }

            // If at least one thread is progressing.
            if(updatedSamples){
                msg.payload.sample.loadPercentage /= updatedSamples;
                msg.payload.sample.latency /= updatedSamples;
                std::cout << "Sending: " << msg.payload.sample << std::endl;
            }else{
                msg.payload.sample.bandwidth = 0;
                msg.payload.sample.latency = std::numeric_limits<double>::max();
                msg.payload.sample.loadPercentage = 0;
                msg.payload.sample.numTasks = 0;
                std::cout << "no updates, Sending: " << msg.payload.sample << std::endl;
            }
            
            // Aggregate custom values.
            if(application->_aggregator){
                std::vector<double> customVec;
                customVec.reserve(application->_threadData.size());
                for(size_t i = 0; i < KNARR_MAX_CUSTOM_FIELDS; i++){
                    customVec.clear();
                    for(ThreadData& td : application->_threadData){
                        customVec.push_back(td.sample.customFields[i]);
                    }
                    msg.payload.sample.customFields[i] = application->_aggregator->aggregate(i, customVec);
                }
            }
            DEBUG(msg.payload.sample);
            // Send message
            application->_channelRef.send(&msg, sizeof(msg), 0);
            // Tell all threads to clear their sample since current one have
            // been already sent.
            for(ThreadData& toClean : application->_threadData){
                toClean.clean = true;
            }
        }else if(res == -1){
            throw std::runtime_error("Received less bytes than expected.");
        }
    }
    return NULL;
}

Application::Application(const std::string& channelName, size_t numThreads,
                         bool quickReply, Aggregator* aggregator):
        _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
        _started(false), _aggregator(aggregator), _executionTime(0), _totalTasks(0),
        _quickReply(quickReply){
    _chid = _channelRef.connect(channelName.c_str());
    assert(_chid >= 0);
    pthread_mutex_init(&_mutex, NULL);
    _supportStop = false;
    _threadData.resize(numThreads);
    // Pthread Create must be the last thing we do in constructor
    pthread_create(&_supportTid, NULL, applicationSupportThread, (void*) this);
}

Application::Application(nn::socket& socket, uint chid, size_t numThreads,
                         bool quickReply, Aggregator* aggregator):
        _channel(NULL), _channelRef(socket), _chid(chid), _started(false),
        _aggregator(aggregator), _executionTime(0), _totalTasks(0),
        _quickReply(quickReply){
    pthread_mutex_init(&_mutex, NULL);
    _supportStop = false;
    _threadData.resize(numThreads);
    // Pthread Create must be the last thing we do in constructor
    pthread_create(&_supportTid, NULL, applicationSupportThread, (void*) this);
}

Application::~Application(){
    if(_channel){
        _channel->shutdown(_chid);
        delete _channel;
    }
}    

void Application::notifyStart(){
    Message msg;
    msg.type = MESSAGE_TYPE_START;
    msg.payload.pid = getpid();
    int r = _channelRef.send(&msg, sizeof(msg), 0);
    assert(r == sizeof(msg));
}

ulong Application::getCurrentTimeNs(){
#ifdef KNARR_NS_PER_TICK 
    return rdtsc() / KNARR_NS_PER_TICK;
#else
    struct timespec tp;
    int r = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(!r);
    return tp.tv_sec * 1.0e9 + tp.tv_nsec;
#endif
}

void Application::updateSamplingLength(ThreadData& tData){
    if(tData.sample.numTasks){
        double latencyNs = tData.sample.latency / tData.sample.numTasks;
        double latencyMs = latencyNs / 1000000.0;
        tData.samplingLength = std::ceil(KNARR_SAMPLING_LENGTH_MS / latencyMs);
    }
}

void Application::begin(uint threadId){
    if(threadId > _threadData.size()){
        throw new std::runtime_error("Wrong threadId specified (greater than number of threads).");
    }
    ThreadData& tData = _threadData[threadId];

    tData.currentSample = (tData.currentSample + 1) % tData.samplingLength;
    
    // Skip
    if(tData.currentSample && tData.currentSample != 1){
        return;
    }
    
    // To collect a sample, we need to execute begin two 
    // times in a row, i.e.
    //     ... begin(); end(); begin(); ...
    //
    // Otherwise it would not be possible to collect the
    // idleTime.
    //
    // When tData.currentSample == 0:
    //      - We start the timer for recording latency 
    //        (tData.computeStart = now).
    // When tData.currentSample == 1:
    //      - We record idleTime (since the timer has)
    //        been started by end() with currentSample == 0.
    // The only exception is for samplingLength == 1, since
    // in this case currentSample is always 0.

    ulong now = getCurrentTimeNs();
    if(!_started){
        pthread_mutex_lock(&_mutex);
        // This awful double check is done to avoid locking the flag
        // every time (this code is executed at most once).
        if(!_started){
            notifyStart();
            _started = true;
        }
        pthread_mutex_unlock(&_mutex);

    }
    if(!tData.firstBegin){
        tData.firstBegin = now;
    }

    if(tData.computeStart && (tData.currentSample == 1 || tData.samplingLength == 1)){
        tData.sample.numTasks += tData.samplingLength;
        tData.totalTasks += tData.samplingLength;

        // If samplingLength is different from 1, we need
        // to add 1 to the tasks count, 
        // since a sample contains 2 begin() calls.
        if(tData.samplingLength != 1){
            ++tData.sample.numTasks;
            ++tData.totalTasks;
        }
        // ATTENTION: IdleTime must be the last thing to set.
        tData.idleTime += ((now - tData.rcvStart) * tData.samplingLength);
#ifdef KNARR_SAMPLING_LENGTH_MS
        updateSamplingLength(tData);
#endif
        // It may seem stupid to clean things after they have just been set,
        // but it is better to do not move earlier the following code.
        if(tData.clean){
            tData.clean = false;
            // Reset fields
            tData.reset();
        }
    }
    tData.computeStart = now;
}

void Application::storeCustomValue(size_t index, double value, uint threadId){
    if(threadId > _threadData.size()){
        throw new std::runtime_error("Wrong threadId specified (greater than number of threads).");
    }
    if(index < KNARR_MAX_CUSTOM_FIELDS){
        ThreadData& tData = _threadData[threadId];
        tData.sample.customFields[index] = value;
    }else{
        throw std::runtime_error("Custom value index out of bound. Please "
                                 "increase KNARR_MAX_CUSTOM_FIELDS macro value.");
    }
}

void Application::end(uint threadId){
    if(threadId > _threadData.size()){
        throw new std::runtime_error("Wrong threadId specified (greater than number of threads).");
    }
    if(!_started){
        throw std::runtime_error("end() called without begin().");
    }
    ThreadData& tData = _threadData[threadId];
    // Skip
    if(tData.currentSample){
        return;
    }
    // We only store samples if tData.currentSample == 0
    ulong now = getCurrentTimeNs();
    tData.rcvStart = now;
    double newLatency = (tData.rcvStart - tData.computeStart);
    // If we perform sampling, we assume that all the other samples
    // different from the one recorded had the same latency.
    tData.sample.latency += (newLatency * tData.samplingLength); 
    tData.lastEnd = now;
}

void Application::terminate(){
    _supportStop = true;
    pthread_join(_supportTid, NULL);
    Message msg;
    msg.type = MESSAGE_TYPE_STOP;
    ulong lastEnd = 0, firstBegin = std::numeric_limits<ulong>::max(); 
    for(ThreadData& td : _threadData){
        _totalTasks += td.totalTasks;
        if(td.firstBegin < firstBegin){
            firstBegin = td.firstBegin;
        }
        if(td.lastEnd > lastEnd){
            lastEnd = td.lastEnd;
        }
    }
    _executionTime = (lastEnd - firstBegin) / 1000000.0; // Must be in ms
    msg.payload.time = _executionTime;
    msg.payload.totalTasks = _totalTasks;
    int r = _channelRef.send(&msg, sizeof(msg), 0);
    assert (r == sizeof(msg));
    // Wait for ack before leaving (otherwise if object is destroyed
    // the monitor could never receive the stop).
    r = _channelRef.recv(&msg, sizeof(msg), 0);
    assert(r == sizeof(msg));
}

ulong Application::getExecutionTime(){
    return _executionTime;
}

unsigned long long Application::getTotalTasks(){
    return _totalTasks;
}

Monitor::Monitor(const std::string& channelName):
        _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
        _executionTime(0), _totalTasks(0){
    _chid = _channelRef.bind(channelName.c_str());
    assert(_chid >= 0);
}

Monitor::Monitor(nn::socket& socket, uint chid):
        _channel(NULL), _channelRef(socket), _chid(chid), _executionTime(0), _totalTasks(0){
    ;
}

Monitor::~Monitor(){
    if(_channel){
        _channel->shutdown(_chid);
        delete _channel;
    }
}

pid_t Monitor::waitStart(){
    Message m;
    int r = _channelRef.recv(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    assert(m.type == MESSAGE_TYPE_START);
    return m.payload.pid;
}

bool Monitor::getSample(ApplicationSample& sample){
    Message m;
    m.type = MESSAGE_TYPE_SAMPLE_REQ;
    int r = _channelRef.send(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    r = _channelRef.recv(&m, sizeof(m), 0);
    assert(r == sizeof(m));
    if(m.type == MESSAGE_TYPE_SAMPLE_RES){
        sample = m.payload.sample;
        return true;
    }else if(m.type == MESSAGE_TYPE_STOP){
        _executionTime = m.payload.time;
        _totalTasks = m.payload.totalTasks;
        // Send ack.
        m.type = MESSAGE_TYPE_STOPACK;
        r = _channelRef.send(&m, sizeof(m), 0);
        assert(r == sizeof(m));
        return false;
    }else{
        throw runtime_error("Unexpected message type.");
    }
}

ulong Monitor::getExecutionTime(){
    return _executionTime;
}

unsigned long long Monitor::getTotalTasks(){
    return _totalTasks;
}

} // End namespace
