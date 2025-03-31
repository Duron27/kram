#include "KramThreadPool.h"

// #include <atomic>
//#include <queue>

#if KRAM_WIN
#include <synchapi.h>
#endif

#if KRAM_LINUX
#include <linux/futex.h>
#endif

// Remove this, and move out all the threading prims
#include "TaskSystem.h"

// Android is missing defines
#if KRAM_ANDROID
#ifndef SYS_futex
# define SYS_futex __NR_futex
#endif
#ifndef FUTEX_WAIT_BITSET
# define FUTEX_WAIT_BITSET 9
#endif
#ifndef FUTEX_WAKE_BITSET
# define FUTEX_WAKE_BITSET 10
#endif
#ifndef FUTEX_PRIVATE_FLAG
# define FUTEX_PRIVATE_FLAG 128
#endif
#endif

// TODO: don't understand how jobs get distributed to the various queues
// Especially if all jobs are coming in from the main/scheduler thread.
//
// TODO: add ability to grow/shrink workers
//
// Inspired by notes from Andreas Fredriksson on building a better thread
// pool. Otherwise, every pool just uses a single cv and mutex.  That cv
// then has to do a lot of wakey wakey when the thread/core counts are high.
//
// Talks about Unity thread pool
// https://unity.com/blog/engine-platform/improving-job-system-performance-2022-2-part-1
//
// https://unity.com/blog/engine-platform/improving-job-system-performance-2022-2-part-2

namespace kram {
using namespace STL_NAMESPACE;


// futex is 0 when not available and 1 when availalb.e

#if KRAM_MAC

// C++20 calls below
// iOS14, macOS 11

void futex::wait(uint32_t expectedValue) {
    auto monitor = __libcpp_atomic_monitor(&_value);
    // Check again if we should still go to sleep.
    if (_value.load() != expectedValue) {
        return;
    }
    // Wait, but only if there's been no new notifications
    // since we acquired the monitor.
    __libcpp_atomic_wait(&_value, monitor);
}

void futex::notify_one() {
    __cxx_atomic_notify_one(&_value);
}

void futex::notify_all() {
    __cxx_atomic_notify_all(&_value);
}


#elif KRAM_WIN

// Win8+

void futex::wait(uint32_t expectedValue) {
    WaitOnAddress(&_value, &expectedValue(), sizeof(uint32_t), INFINITE);
}

void futex::notify_one() {
    WakeByAddressSingle(&_value)
}

void futex::notify_all() {
    WakeByAddressAll(&_value);
}


#elif KRAM_LINUX || KRAM_ANDROID

// Linux 2.6.7
// Only has uint32_t support

void futex::wait(uint32_t expectedValue) {
    syscall(SYS_futex, &_value, FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG,
            NULL, NULL, expectedValue);
}

void futex::notify_one() {
    syscall(SYS_futex, &_value, FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAT,
            NULL, NULL, 1);
}

void futex::notify_all() {
    syscall(SYS_futex, &value, FUTEX_WAKE_BITSET | FUTEX_PRIVATE_FLAT,
            NULL, NULL, INT32_MAX); // TODO: UINT32_MAX?
}

#endif

// Each thread has it's own queue

// main thread is responsible for making sure one thread is awake
// when it schedules a job.
// but main no longer has to drive waking, since each worker can

Scheduler::Scheduler(uint32_t numWorkers) {
    // TODO: numWorkers must be even number?  If buddy are paired
    // if each worker uses next as buddy, then can be odd.
    
    _schedulerThread = getCurrentThread();
    
    // TODO: should move the scheduler settings out
    ThreadInfo infoMain = {"Sheduler", ThreadPriority::Interactive, 0};
    setThreadInfo(infoMain);
    
    string name;
    
    for (uint32_t threadIndex = 0; threadIndex < numWorkers; ++threadIndex) {
        // These have to be ptr, due to uncopyable futex/mutex
        Worker* worker = new Worker();
        sprintf(name, "Task%d", threadIndex);
        worker->Init( name, threadIndex, this);
        
        _workers.push_back(worker);
    }
    
    
    // Note that running work on core0 when core0 may starve it
    // from assigning work to threads.
    
    // start up the threads
    
    for (uint32_t threadIndex = 0; threadIndex < numWorkers; ++threadIndex) {
        // Generate a name, also corresponds to core for affinity
        // May want to include priority too.
        Worker& worker = *_workers[threadIndex];
        
        _threads.emplace_back([threadIndex, &worker] {
            
            // This is setting affinity to threadIndex
            ThreadInfo infoTask = {worker._name.c_str(), ThreadPriority::High, (int)threadIndex};
            setThreadInfo(infoTask);
            
            worker.run();
        });
    }
}


// TODO: this is one way to pass pririty with priority
//    template<typename F>
//    void scheduleJob(int priority, F f) {
//        sheduleJob(Job2(priority, f));
//    }

void Scheduler::scheduleJob(Job2& job) {
    auto currentThread = getCurrentThread();

    // Already on same thread.  That thread is awake.
    // But another thread could be stealing a job,
    // so for now project queue with mutex.
    if (currentThread == _schedulerThread) {
        // TODO: need to pick a best queue to put work on?
        // otherwise everything gets stuck on scheduler queue
        // and then is stolen off it.
        
        // Scheduler thread needs to ensure a worker is awake
        // since it doesn't run it's own work?
        {
            Worker* worker = _workers[0];
            
            lock_guard<mutex> lock(worker->_mutex);
            worker->_queue.push(std::move(job));
            _stats.jobsTotal++;
        }
        
        _workers[0]->_futex.notify_one();
    }
    else {
        Worker* worker = findWorker(currentThread);
        if (!worker)
            return;
        
        lock_guard<mutex> lock(worker->_mutex);
        worker->_queue.push(std::move(job));
        _stats.jobsTotal++;
    }
}

void Scheduler::stop()
{
    // has to be called on scheduler thread
    KASSERT(getCurrentThread() == _schedulerThread);
    
    if (_isStop)
        return;
    
    _isStop = true;
    
    // have all threads wait on each other to finish
    for (uint32_t i = 0; i < _workers.size(); ++i) {
        _workers[i]->_futex.notify_one();
        _threads[i].join();
        
        // since had to use ptrs
        delete _workers[i];
    }
}
   

bool Worker::stealFromOtherQueues(Job2& job)
{
    bool found = false;
    
    // TODO: this is always going to vist 0...n
    for (Worker* worker: _scheduler->workers()) {
        if (worker == this)
            continue;
        
        // lots of mutex locks
        lock_guard<mutex> lock(worker->_mutex);
        if (!worker->_queue.empty()) {
            job = std::move(worker->_queue.top());
            worker->_queue.pop();
            
            SchedulerStats& stats = _scheduler->stats();
            stats.jobsExecuting++;
            found = true;
            break;
        }
    }
    
    return found;
}

void Worker::wakeWorkers()
{
    // This takes responsibility off the main thread
    // to keep waking threads to run tasks.
    auto& workers = _scheduler->workers();
    
    Worker* buddy = workers[(_workerId+1) % workers.size()];
    
    if (!buddy->_isExecuting) {
        buddy->_futex.notify_one();
        return;
    }
    
    // TODO: should we only wake as many workers as jobs
    // what if they are already awake and working?
    // uint32_t numJobs = _scheduler->stats().jobsRemaining();
    
    // Wrap around visit from just past buddy
    for (uint32_t i = 0; i < workers.size()-2; ++i) {
        Worker* worker = workers[(_workerId+2+i) % workers.size()];
        if (!worker->_isExecuting) {
            worker->_futex.notify_one();
            break;
        }
    }
}

bool Worker::shouldSleep()
{
    // TODO: needs to be more complex
    // for parallel task exectution.
    
    return true;
}

void Worker::run()
{
    SchedulerStats& stats = _scheduler->stats();
    
    while(!_scheduler->isStop()) {
        // Take a job from our worker thread’s local queue
        Job2 job;
        bool found = false;
        {
            lock_guard<mutex> lock(_mutex);
            if (!_queue.empty()) {
                job = std::move(_queue.top());
                _queue.pop();
                stats.jobsExecuting++;
                found = true;
            }
        }
        
        // If our queue is empty try to steal work from someone
        // else's queue to help them out.
        if(!found) {
            found = stealFromOtherQueues(job);
        }
        
        if(found) {
            // If we found work, there may be more conditionally
            // wake up other workers as necessary
            wakeWorkers();
            
            // Any job spawned by job goes to same queue.
            // But may get stolen by another thread.
            // Try not to have tasks wait on sub-tasks
            // or their thread is locked down.
            _isExecuting = true;
            job.execute();
            _isExecuting = false;
            
            // these can change a little out of order
            stats.jobsExecuting--;
            stats.jobsTotal--;
        }
        
        // Conditionally go to sleep (perhaps we were told there is a
        // parallel job we can help with)
        else if(shouldSleep()) {
            // Put the thread to sleep until more jobs are scheduled
            _futex.wait(1); // TODO: fix wait to not take value
        }
    }
}

} // namespace kram
