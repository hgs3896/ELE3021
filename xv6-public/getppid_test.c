#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char* argv[])
{
    int pid, ppid;

    printf(1, "My PID: %d\n", (pid = getpid()));
    printf(1, "My PPID: %d\n", getppid());
    
    if ( (pid  = fork()) < 0 ) {
        exit();
    }

    if ( pid == 0 ){
        pid = getpid();
        ppid = getppid();
        printf(1, "[Child] My PID: %d\n", pid);
        printf(1, "[Child] My PPID: %d\n", ppid);
        exit();
    }
    ppid = getppid();
    
    wait();

    if(pid != ppid){
        printf(1, "get ppid successful\n");
    }
    exit();
}