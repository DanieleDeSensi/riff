#include "external/cppnanomsg/nn.hpp"
#include "external/nanomsg/src/pair.h"

#include "knarr.hpp"

#include <cmath>
#include <errno.h>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#undef DEBUG
#undef DEBUGB

#ifdef DEBUG_knarr
#define DEBUG(x) do { std::cerr << "[Knarr] " << x << std::endl; } while (0)
#define DEBUGB(x) do {x;} while (0)
#else
#define DEBUG(x)
#define DEBUGB(x)
#endif

namespace knarr{

Application::Application(const std::string& channelName):
	    _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
	    _started(false), _rcvStart(0),
        _computeStart(0), _idleTime(0), _firstBegin(0), _lastEnd(0){
    _chid = _channelRef.connect(channelName.c_str());
    assert(_chid >= 0);
}

Application::Application(nn::socket& socket, uint chid):
        _channel(NULL), _channelRef(socket), _chid(chid), _started(false),
        _rcvStart(0), _computeStart(0), _idleTime(0), _firstBegin(0), _lastEnd(0){
    ;
}

Application::~Application(){
    if(_channel){
        _channel->shutdown(_chid);
        delete _channel;
    }
}    

void Application::notifyStart(){
    _sentMsg.type = MESSAGE_TYPE_START;
    _sentMsg.payload.pid = getpid();
    assert(_channelRef.send(&_sentMsg, sizeof(_sentMsg), 0) == sizeof(_sentMsg));
}

ulong Application::getCurrentTimeNs(){
    struct timespec tp;
    assert(!clock_gettime(CLOCK_MONOTONIC, &tp));
    return tp.tv_sec * 1.0e9 + tp.tv_nsec;
}

void Application::begin(){
    ulong now = getCurrentTimeNs();
    if(!_started){
        notifyStart();
        _started = true;
        _firstBegin = now;
    }
    _lastEnd = now;
    if(_computeStart){
        if(_rcvStart){
            _idleTime += (now - _rcvStart);
        }else{
            _currentMsg.payload.sample.latency += (now - _computeStart);
        }
        ++_currentMsg.payload.sample.tasksCount;
        ulong totalTime = (_currentMsg.payload.sample.latency + _idleTime);
        _currentMsg.payload.sample.bandwidthTotal = (double) _currentMsg.payload.sample.tasksCount / totalTime;
        _currentMsg.payload.sample.loadPercentage = ((double) _currentMsg.payload.sample.latency / totalTime) * 100.0;
        _currentMsg.payload.sample.latency /= (double) _currentMsg.payload.sample.tasksCount;
        _currentMsg.type = MESSAGE_TYPE_SAMPLE_RES;
        DEBUG(_currentMsg.sample);
        _computeStart = now;

        Message m;
        int res = _channelRef.recv(&m, sizeof(m), NN_DONTWAIT);
        if(res == sizeof(m)){
            assert(m.type == MESSAGE_TYPE_SAMPLE_REQ);
            m.type = MESSAGE_TYPE_SAMPLE_RES;
            _sentMsg = _currentMsg;
            m = _sentMsg;
            _channelRef.send(&m, sizeof(m), 0);

            _currentMsg.payload.sample.tasksCount = 0;
            _currentMsg.payload.sample.loadPercentage = 0;
            _currentMsg.payload.sample.latency = 0;
            _currentMsg.payload.sample.bandwidthTotal = 0;
            _rcvStart = 0;
            //_computeStart = 0;
            _idleTime = 0;
        }else if(res == -1 && errno != EAGAIN){
            throw std::runtime_error("Unexpected error on recv");
        }else if(res != -1){
            throw std::runtime_error("Received less bytes than expected.");
        }
    }else{
        _computeStart = now;
    }
}

void Application::end(){
    if(!_started){
        notifyStart();
        _started = true;
    }
    _rcvStart = getCurrentTimeNs();
    if(_computeStart){
        _currentMsg.payload.sample.latency += (_rcvStart - _computeStart);
    }
    _lastEnd = _rcvStart;
}

void Application::terminate(){
    _sentMsg.type = MESSAGE_TYPE_STOP;
    _sentMsg.payload.time = _lastEnd - _firstBegin;
    assert(_channelRef.send(&_sentMsg, sizeof(_sentMsg), 0) == sizeof(_sentMsg));
}

Monitor::Monitor(const std::string& channelName):
        _channel(new nn::socket(AF_SP, NN_PAIR)), _channelRef(*_channel),
        _executionTime(0){
    _chid = _channelRef.bind(channelName.c_str());
    assert(_chid >= 0);
}

Monitor::Monitor(nn::socket& socket, uint chid):
        _channel(NULL), _channelRef(socket), _chid(chid), _executionTime(0){
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
    assert(_channelRef.recv(&m, sizeof(m), 0) == sizeof(m));
    assert(m.type == MESSAGE_TYPE_START);
    return m.payload.pid;
}

bool Monitor::getSample(ApplicationSample& sample){
    Message m;
    m.type = MESSAGE_TYPE_SAMPLE_REQ;
    assert(_channelRef.send(&m, sizeof(m), 0) == sizeof(m));
    assert(_channelRef.recv(&m, sizeof(m), 0) == sizeof(m));
    if(m.type == MESSAGE_TYPE_SAMPLE_RES){
        sample = m.payload.sample;
        return true;
    }else if(m.type == MESSAGE_TYPE_STOP){
        _executionTime = m.payload.time;
        return false;
    }else{
        throw runtime_error("Unexpected message type.");
    }
}

ulong Monitor::getExecutionTime(){
    return _executionTime;
}

} // End namespace
