#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>


namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    volatile char a;
    ctx.Low=(char*)(&a);
    if (std::get<0>(ctx.Stack) != nullptr){
        delete[] std::get<0>(ctx.Stack);
    }
    ctx.Stack=std::make_tuple(new char[ctx.High-ctx.Low], ctx.High-ctx.Low);
    memcpy(std::get<0>(ctx.Stack),ctx.Low, ctx.High-ctx.Low);
}

void Engine::Restore(context &ctx) {
    volatile char a;
    if (ctx.Low <= &a && &a <= ctx.High) {
        Restore(ctx);
    }
    memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.High-ctx.Low);
    longjmp(ctx.Environment,1);
}

void Engine::yield() {
    if (alive==nullptr){
        return;
    }
    sched(alive);
}

void Engine::sched(void *routine_) {
    if (routine_==cur_routine){
        return;
    }
    if (routine_==nullptr){
        yield();
    }
    if (cur_routine != idle_ctx && setjmp(cur_routine->Environment) > 0){
        return;
    }
    Store(*cur_routine);
    cur_routine=(context*)routine_;
    Restore(*(context*)(routine_));
}

} // namespace Coroutine
} // namespace Afina
