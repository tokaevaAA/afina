#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    volatile char a;
    ctx.Low = (char*)(&a);
    auto &stack_ptr = std::get<0>(ctx.Stack);
    auto &stack_size = std::get<1>(ctx.Stack);
    auto required_size = ctx.High - ctx.Low;
    if (stack_ptr == nullptr || stack_size < required_size || stack_size >= 2 * required_size) {
        delete[] stack_ptr;
        ctx.Stack=std::make_tuple(new char[required_size],required_size);
        //stack_ptr = new char[required_size];
        //stack_size = required_size;
    }
    memcpy(std::get<0>(ctx.Stack), ctx.Low, ctx.High - ctx.Low);
}

void Engine::Restore(context& ctx) {
    //if we call Restore for idle_ctx, we want to enlarge stack not to hit smb's stack
    volatile char a;
    if (&ctx != idle_ctx && ctx.Low <= &a && &a <= ctx.High) {
        Restore(ctx);
    }
    if (&ctx != idle_ctx){
        cur_routine = &ctx;
        memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.High - ctx.Low);
    }
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    if (alive==nullptr){
        return;
    }
    if (alive != cur_routine){ sched(alive);} //we wont return from sched
    else if (alive->next != nullptr) {sched(alive->next);}
    //if here, then return 
    
}

void Engine::sched(void *routine_) {
    if (routine_==cur_routine){
        return;
    }
    if (routine_==nullptr){
        yield(); //go to yield,here cur_routine!=nullptr (because otherwise cur_routine=nullptr=routine_, hence we would have gone to 51 ) 
                // if alive==nullptr, then return, no recursion
                // if (alive != cur_routine){ sched(alive);} but alive !=nullptr, !=cur_routine then go to 62, no recursion
                //if (alive == cur_routine), but alive->next != nullptr, alive->next != cur_routine, because alive==cur_rountine, then 62
                //if (alive == cur_routine), but alive->next == nullptr, then return, no recursion
        return;
    }
    if (cur_routine == nullptr){ //here we want to restore idle_ctx
        Restore(*(context*)(routine_));
        return;
    }

    if (setjmp(cur_routine->Environment) > 0){
        return;
    }
    
    Store(*cur_routine);
    Restore(*(context*)(routine_));
}

} // namespace Coroutine
} // namespace Afina
