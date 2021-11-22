#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>


namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    volatile char a;
    ctx.Low=(char*)(&a);
    if (std::get<0>(ctx.Stack) != nullptr && (
        std::get<1>(ctx.Stack) < ctx.High-ctx.Low  || std::get<1>(ctx.Stack) > 4*(ctx.High-ctx.Low ))){
        delete[] std::get<0>(ctx.Stack);
        ctx.Stack=std::make_tuple(new char[2*(ctx.High-ctx.Low)], 2*(ctx.High-ctx.Low));
    }
    if (std::get<0>(ctx.Stack) == nullptr){
        ctx.Stack=std::make_tuple(new char[2*(ctx.High-ctx.Low)], 2*(ctx.High-ctx.Low));
    }
    memcpy(std::get<0>(ctx.Stack),ctx.Low, ctx.High-ctx.Low);
}

void Engine::Restore(context& ctx) {
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
    if (alive != cur_routine){ sched(alive);}
    if (alive->next != nullptr) {sched(alive->next);}
    else {return;}
}

void Engine::sched(void *routine_) {
    if (routine_==cur_routine){
        return;
    }
    if (routine_==nullptr){
        yield();
        return;
    }
    if (cur_routine == nullptr){
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
