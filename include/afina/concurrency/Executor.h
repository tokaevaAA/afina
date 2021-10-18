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
     _name(std::move(name), _max_queue_size(max_queue_size), _low_watermark(low_watermark), _high_watermark(high_watermark), _idle_time(idle_time) ){
         std::unique_lock<std::mutex> mylock(this->mutex);
         for (int i=0; i<_low_watermark; i=i+1){
             threads.emplace_back(std::thread([this] {return perform(this);}));
         }

    }
    ~Executor(){
        std::unique_lock<std::mutex> mylock(this->mutex);
        if (state != Executor::State::kStopped){
            Stop(true);
        }
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false){
        std::unique_lock<std::mutex> mylock(this->mutex);
        state=Executor::State::kStopping;
        empty_condition.notify_all();
        tasks.clear();
        if (await){
            while(!threads.empty()){
                _cv_no_more_threads.wait(mylock);
            }
        }
        
    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun || tasks.size() == _max_queue_size) {
            return false;
        }

        // Enqueue new task
        std::unique_lock<std::mutex> mylock(this->mutex);
        if ((tasks.size() > threads.size()) && (threads.size() < _high_watermark){
            threads.emplace_back(std::thread([this] {return perform(this);}));
        }
        tasks.push_back(exec);
        empty_condition.notify_one();
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
    friend void perform(Executor *executor){
        //myfunc
        auto func_finish_thread=[executor]{
            auto this_thread=std::this_thread::get_id();
            for (auto it=executor->threads.begin(); it<executor->threads.end(); i=i+1){
                if (it->get_id()==this_thread){
                    (*it).detach();
                    executor->threads.erase(it);
                    if (executor->threads.empty()){
                        executor->state=Executor::State::kStopped;
                        executor->_cv_no_more_threads.notify_all();
                    }
                    return;
                }
            }
        };
        std::function<void()> task;
        while (executor->state == Executor::State::kRun){
            {
               std::unique_lock<std::mutex> mylock(executor->mutex);
               while (executor->tasks.empty()){
                   executor->empty_condition.wait_for(mylock, std::chrono::milliseconds(executor->idle_time));
                   if (executor->tasks.empty() && executor->threads.size() > executor->_low_watermark){
                       func_finish_thread();
                       return;
                   } 
               } 
               task=std::move(executor->tasks.front());
               executor->tasks.pop_front();
            }
            try{task();}
            catch(const std::exception& myex){
                std::cout<<myex.what()<<std::endl;
            }
        }
        func_finish_thread();
    }

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;
    std::string _name;
    std::size_t _low_watermark;
    std::size_t _high_watermark;
    std::size_t _max_queue_size;
    std::size_t _idle_time;
    std::condition_variable _cv_no_more_threads;

};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
