#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <type_traits>
#include <string>
#include <cassert>
#include <chrono>
#include <fstream>
#include <stdexcept>

using namespace std::chrono_literals;

//I did'nt implement formal Unit Tests because I had not enough time
// but I did a kind of UTs using assert
// 
// NOTICE: I implemented the lock primitive with a configurable timeout (5secs default)
// after that timeout lock throws an exception on the wait trhread
// Thats is because I think is better in order to debug and avoid
// that bad used of lock/unlocks ends in a deadlock, always is better to
// throw an exception if something is wrong instead do nothing.
// In any case that exception could be cached and and maybe recover from the problem 
// 
// I tested this in linux and windows
// 
//#define DEBUG

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#ifdef DEBUG
#define DBG(str)    std::cout << str << std::endl;
#else
#define DBG(str)    ;
#endif

// Make this class able to have derived classes.
class Lock {
public:
    virtual void lock()=0;
    virtual void unlock()=0; 

 
protected:
    std::mutex _mutex;
};


// Your implementation. Complete missing elements.
class ReentrantLock : public Lock {

public:
    virtual void lock() noexcept(false) override;
    virtual void unlock() override;
    auto getCount() const { return _count; }
    auto getOwner() const { return _owner; }
    void setLockTimeout(std::chrono::seconds t) {
        _lockTimeout = t;
    }
 
 protected:
    std::chrono::seconds _lockTimeout = std::chrono::seconds(5);
    uint16_t _count = 0;
    std::thread::id _owner;
    std::condition_variable _cv;
};

void 
ReentrantLock::lock() noexcept(false) {
    const auto pid = std::this_thread::get_id();
    DBG(pid << ", " << __PRETTY_FUNCTION__);
    std::unique_lock<std::mutex> _lk(_mutex);
    
    if (_count == 0) {
        // recursive mutex is free, so I take it
        _count=1;
        _owner = pid;
        DBG(pid << ", mtx is free, got it, count: " << _count);
        return;
    }

    if ( _owner == pid) { 
        // recursive mutex was mine, so I keep it
        ++_count;
        DBG(pid << ", mtx is still mine, count: " << _count);
        return;
    }

    // recursive mutex is taken by some other thread
    DBG (pid <<", mtx is taked by thr("<<_owner<<"), count: " << _count);

    if (_cv.wait_for(_lk, _lockTimeout, [&] {return _count == 0; })) {
        ++_count;
        _owner = pid;
        DBG( pid << ", finished waiting, mtx is mine now, count:" << _count);
    }
    else {
        std::cerr << pid << " timed out Error! count: " << _count << '\n';
        throw std::runtime_error("timeod out wait");
    }
}
 
void 
ReentrantLock::unlock() {
    const auto pid = std::this_thread::get_id();
    DBG(pid << ", " << __PRETTY_FUNCTION__);

    std::unique_lock<std::mutex> guard(_mutex);
    // not mine, bugger off
    if (_count>0 && _owner != pid) {
        DBG(pid << ", [unlock] mtx is taked by thr("<<_owner<<")");
        return;
    }

    if (_count>0){
        _count--;
        DBG(pid << ", [unlock], count: " << _count);
        if (_count == 0) {
            DBG(pid << ", count reachs 0, notifing...");
            _cv.notify_one();
        }
        return;
    }
    std::cerr << pid << ", Error!! unlock called before lock..\n";
}

class SharedClass {
public:
    SharedClass(Lock *lock):_lock(lock){}

    //k is the number of times that calls to itself recursively
    void functionA (uint16_t k=0) {
        const auto pid = std::this_thread::get_id();
        DBG(pid << ", " << __PRETTY_FUNCTION__);
        assert(_lock != nullptr);
        _lock->lock();
        _sharedStr = "functionA-" + std::to_string(k);
        std::cout << pid <<", in functionA, shared variable is now " << _sharedStr << '\n';
     
        if (k > 0) functionA(--k);//reentrant, when k reachs 0 it returns
        _lock->unlock();
    }
  
    void functionB () {
        const auto pid = std::this_thread::get_id();
        DBG(pid << ", " << __PRETTY_FUNCTION__);
        assert(_lock != nullptr);
        _lock->lock();
        _sharedStr = "functionB";
        std::cout << pid << ", in function B, shared variable is now " << _sharedStr << '\n';
        functionA();
        std::cout <<pid<<", back in functionB, shared variable is " << _sharedStr << '\n';
        _lock->unlock();
    };

    //just for test
    void fTestA(uint16_t k = 0) {
        const auto pid = std::this_thread::get_id();
        DBG(pid << ", " << __PRETTY_FUNCTION__);
        assert(_lock != nullptr);
        _lock->lock();
        _sharedStr = "TEST_A-" + std::to_string(k);
        std::cout << pid << ", in fTestA, shared variable is now " << _sharedStr << '\n';
        std::this_thread::sleep_for(5550ms);
        if (k > 0) functionA(k - 1);
        _lock->unlock();
    }

    void fTestB(bool &result) {
        result = false;
        try {
            const auto pid = std::this_thread::get_id();
            DBG(pid << ", " << __PRETTY_FUNCTION__);
            assert(_lock != nullptr);
            _lock->lock();
            _sharedStr = "TEST_B";
            std::cout << pid << ", in fTestB, shared variable is now " << _sharedStr << '\n';
            fTestA();
            std::cout << pid << ", back in fTestB, shared variable is " << _sharedStr << '\n';
            _lock->unlock();
        }
        catch (std::exception& e) {
            std::cerr << "fTestB exception: " << e.what() << std::endl;
            result=true;
        }
    };
private:
    Lock *_lock;
    std::string _sharedStr;
};


int main() {
    ReentrantLock lock; 

    SharedClass sharedInstance(&lock);
    {
        std::cout << "\nfirst test (reentrant)...\n";
  
        std::thread t1(&SharedClass::functionA, &sharedInstance, 3);
        std::thread t2(&SharedClass::functionB, &sharedInstance);

        t1.join();
        t2.join();
        assert(lock.getCount() == 0);
    }

    {
        std::cout << "\n\nsecond test (original)...\n";

        std::thread t1(&SharedClass::functionA, &sharedInstance, 0);
        std::thread t2(&SharedClass::functionB, &sharedInstance);

        t1.join();
        t2.join();
        assert(lock.getCount() == 0);
    }
    
    {
        std::cout << "\n\nthird test (2 guards)...\n";
        std::lock_guard<ReentrantLock> myGuard(lock);
        std::lock_guard<ReentrantLock> myGuard2(lock);
        assert(lock.getCount() == 2);
    }
    assert(lock.getCount() == 0);

    {
        std::cout << "\nlast test...\n";

        std::thread t1(&SharedClass::fTestA, &sharedInstance, 0);
        bool st = false;
        std::thread t2(&SharedClass::fTestB, &sharedInstance, std::ref(st));
        std::this_thread::sleep_for(500ms);
        //assert(t1.get_id() == lock.getOwner());
        t1.join();
        t2.join();
        assert(st);
        assert(lock.getCount() == 0);
    }

    
    return EXIT_SUCCESS;
}
