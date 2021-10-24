#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor(std::string name, std::size_t max_queue_size, std::size_t low_watermark, std::size_t high_watermark, std::size_t idle_time):
     _name(std::move(name)), _max_queue_size(max_queue_size), _low_watermark(low_watermark), _high_watermark(high_watermark), _idle_time(idle_time) {
         std::unique_lock<std::mutex> mylock(_mutex);
         _state=State::kRun;
     }

    
    void Start(){
        //printf("In start!\n");
        std::unique_lock<std::mutex> mylock(_mutex);
        for (int i=0; i<_low_watermark; i=i+1){
            _threads.emplace_back(std::thread([this] {return perform(this);}));
        }
    }

    ~Executor(){}

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = true){
        //printf("In stop!\n");
        std::unique_lock<std::mutex> mylock(_mutex);
        _state=Executor::State::kStopping;
        _empty_condition.notify_all();
        if (await){
            while(!_threads.empty()){
                _cv_wants_to_stop.wait(mylock);
            }
        }
        _state=Executor::State::kStopped;
        //printf("tasks.size=%ld\n",_tasks.size());
        
    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        //printf("In execute!\n");
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> mylock(_mutex);
        if (_state != State::kRun || _tasks.size() == _max_queue_size) {
            //printf("In execute false!\n");
            return false;
        }
        
        // Enqueue new task
        
        if ((_tasks.size() > _threads.size()) && (_threads.size() < _high_watermark)){
            _threads.emplace_back(std::thread([this] {return perform(this);}));
        }
        //printf("In execute push task!\n");
        _tasks.push_back(exec);
        _empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    
    class SpecialClass{
    private:
        Executor* executor;
    public:
        SpecialClass(Executor* myexecutor){executor=myexecutor;}
        ~SpecialClass(){
            auto this_thread=std::this_thread::get_id();
            for (auto it=executor->_threads.begin(); it<executor->_threads.end(); it=it+1){
                if (it->get_id()==this_thread){
                    (*it).detach();
                    //printf("detach\n");
                    executor->_threads.erase(it);
                    executor->_cv_wants_to_stop.notify_all();
                    return;
                }
            }
        }

    };
    friend void perform(Executor *executor){
        auto myobj=SpecialClass(executor);

        bool flagisrunning=true;
        while (flagisrunning){
            std::function<void()> task;
            {
               std::unique_lock<std::mutex> mylock(executor->_mutex);
               while (executor->_tasks.empty()){
                   executor->_empty_condition.wait_for(mylock, std::chrono::milliseconds(executor->_idle_time));
                   //printf("woke up  %d, %d,  %ld!\n", std::this_thread::get_id(), executor->_tasks.empty(), executor->_tasks.size());
                   if (executor->_tasks.empty() && executor->_threads.size() > executor->_low_watermark){
                       return;
                   } 
                   if (executor->_tasks.empty() && executor->_state!=Executor::State::kRun){return;}
               } 
               task=std::move(executor->_tasks.front());
               executor->_tasks.pop_front();
               flagisrunning=executor->_state==Executor::State::kRun;
            }
            
            try{task();}
            catch(const std::exception& myex){
                //printf("%s\n",myex.what());
                std::terminate;
            }
        }
        std::unique_lock<std::mutex> mylock(executor->_mutex);
        //and here work destructor for SpecialClass
    }

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex _mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable _empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> _threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> _tasks;

    /**
     * Flag to stop bg threads
     */
    State _state;
    std::string _name;
    std::size_t _low_watermark;
    std::size_t _high_watermark;
    std::size_t _max_queue_size;
    std::size_t _idle_time;
    std::condition_variable _cv_wants_to_stop;

};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
