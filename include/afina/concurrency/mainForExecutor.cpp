#include <stdio.h>
#include <stdlib.h>
#include "Executor.h"
#include <unistd.h>

int main(){
    printf("hello!\n");
    Afina::Concurrency::Executor myexecutor("myexec",1000,3, 5,1000);
    myexecutor.Start();
    for (int i=0; i<100;i=i+1){
        myexecutor.Execute([i]{ printf("func %d\n",i); });
    }
    sleep(1);
    myexecutor.Stop();

    
    printf("goodbye!\n");
    return 0;

}