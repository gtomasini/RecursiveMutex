#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <type_traits>
#include <cassert>

auto const mainPID(std::this_thread::get_id());

// Make this class able to have derived classes.
class Lock {
public:

    virtual void lock()=0;
    virtual void unlock(); 
    ~Lock() {}
protected:
    std::mutex _mutex;
};

void
Lock::lock(){
    _mutex.lock();
}

void
Lock::unlock() {
    _mutex.unlock();
}
 
// Your implementation. Complete missing elements.
class ReentrantLock : public Lock {

public:
    ReentrantLock():_count(0),_ownerTID(mainPID) {};
    virtual ~ReentrantLock(){}
    void lock() override;
    void unlock() override;

protected:
    std::atomic<short> _count; ///< Informative. My be used with explicit scoping 
    std::atomic<std::thread::id> _ownerTID;
    std::condition_variable _mutex2beReleased;
};

void 
ReentrantLock::lock() {
    std::unique_lock<std::mutex> guard(_mutex);

    if (_count==0) {
        // recursive mutex is free, take it
        ++_count;
        _ownerTID = std::this_thread::get_id();
        return;
    }

    if ( _ownerTID == std::this_thread::get_id()) { 
        // recursive mutex is mine
        ++_count;
        return;
    }

    // recursive mutex is taken by some other thread
    while (_count>0) {
        _mutex2beReleased.wait (guard);
        // someone has released it. Is it still free?
        if (_count==0) {
            ++_count;
            _ownerTID = std::this_thread::get_id();
            return;
        }
    }

}
 
void 
ReentrantLock::unlock() {
    std::unique_lock<std::mutex> guard(_mutex);

    // not mine, bugger off
    if ( _ownerTID != std::this_thread::get_id()) {
        return;
    }

    _count--;
    if (_count==0)
        _mutex2beReleased.notify_one();
}

class SharedClass {
public:
    SharedClass(Lock* lock):_lock(lock){}

    void functionA(int k=0) {
        assert(_lock != nullptr);

        _lock->lock();
        _sharedStr = "functionA";
        std::cout << "in functionA, shared variable is now " << _sharedStr << '\n';
        _lock->unlock();
    }
  
    void functionB(int k=0) {
        assert(_lock != nullptr);

        _lock->lock();
        _sharedStr = "functionB";
        std::cout << "in functionB, shared variable is now " << _sharedStr << '\n';
        functionA();
        std::cout << "back in functionB, shared variable is " << _sharedStr << '\n';
        _lock->unlock();
    };

private:
    Lock *_lock;
    std::string _sharedStr;
};
 
int main() {

  ReentrantLock lock; 
  {
    std::lock_guard<ReentrantLock> guard0(lock);
    std::lock_guard<ReentrantLock> guard1(lock);
    std::lock_guard<ReentrantLock> guard2(lock);
  }

  SharedClass sharedInstance(&lock);
  
  std::thread t1(&SharedClass::functionA, &sharedInstance);
  std::thread t2(&SharedClass::functionB, &sharedInstance);
  
  t1.join();
  t2.join();

  return EXIT_SUCCESS;
}
