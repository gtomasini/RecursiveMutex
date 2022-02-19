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

using namespace std::chrono_literals;

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

    virtual void lock();
    virtual void unlock(); 
    ~Lock() {}
protected:
    std::mutex _mutex;
};

void Lock::lock(){
    _mutex.lock();
}

void Lock::unlock() {
    _mutex.unlock();
}
 
// Your implementation. Complete missing elements.
class ReentrantLock : public Lock {

public:
    ReentrantLock(){};
    virtual ~ReentrantLock(){}
    void lock() override;
    void unlock() override;

    bool getTimedOut() {
        auto tmp = timed_out;
        timed_out = false;
        return tmp;
    }

protected:
    uint16_t _count=0; ///< Informative. My be used with explicit scoping 
    std::thread::id _ownerTID;
    std::condition_variable _cv;
    bool   timed_out=false;
};

void 
ReentrantLock::lock() {
    const auto pid = std::this_thread::get_id();
    std::unique_lock<std::mutex> _lk(_mutex);

    DBG(pid << ", "<<__PRETTY_FUNCTION__);
    
    if (_count == 0) {
        // recursive mutex is free, so take it
        ++_count;
        _ownerTID = pid;
        DBG(pid << ", mtx is free, got it, count: " << _count);
        return;
    }

    if ( _ownerTID == std::this_thread::get_id()) { 
        // recursive mutex was mine
        ++_count;
        DBG(pid << ", mtx is still mine, count: " << _count);
        return;
    }

    DBG (pid <<", mtx is taked by thr("<<_ownerTID<<"), count: " << _count);
    // recursive mutex is taken by some other thread

    //5 secs of timeout...
    if (_cv.wait_for(_lk, 5000ms, [&] {return _count == 0; })) {
        ++_count;
        _ownerTID = pid;
        DBG( pid << ", finished waiting, mtx is mine now, count:" << _count);
    }
    else {
        std::cerr << pid << " timed out Error! count: " << _count << '\n';
        timed_out = true;
    }
}
 
void 
ReentrantLock::unlock() {
    const auto pid = std::this_thread::get_id();

    std::unique_lock<std::mutex> guard(_mutex);

    DBG(pid << ", " << __PRETTY_FUNCTION__);

    // not mine, bugger off
    if (_count>0 && _ownerTID != std::this_thread::get_id()) {
        DBG(pid << ", [unlock] mtx is taked by thr("<<_ownerTID<<")");
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
    SharedClass(Lock* lock):_lock(lock){}

    void functionA (uint8_t k=0) {
        const auto pid = std::this_thread::get_id();
        DBG(pid << ", " << __PRETTY_FUNCTION__);
        assert(_lock != nullptr);
        _lock->lock();
        _sharedStr = "functionA-x";
        _sharedStr[10] = '0' + k;
        std::cout << pid <<", in functionA, shared variable is now " << _sharedStr << '\n';
     
        if ( k > 0) functionA(k - 1);
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

private:
    Lock *_lock;
    std::string _sharedStr;
};
 
int main() {
  ReentrantLock lock; 

  SharedClass sharedInstance(&lock);
  {
      std::cout << "\nfirst test...\n";
  
      std::thread t1(&SharedClass::functionA, &sharedInstance, 3);
      std::thread t2(&SharedClass::functionB, &sharedInstance);
  
      t1.join();
      t2.join();
  }

  {
      std::cout << "\n\nsecond test...\n";

      std::thread t1(&SharedClass::functionA, &sharedInstance, 0);
      std::thread t2(&SharedClass::functionB, &sharedInstance);

      t1.join();
      t2.join();
  }

  return EXIT_SUCCESS;
}
