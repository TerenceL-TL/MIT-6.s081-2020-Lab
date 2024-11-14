#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char* argv[])
{
    if(argc != 1)
    {
        fprintf(2, "Usage: pingpong\n");
        exit(1);
    }
    int c2f[2];
    int f2c[2];
    pipe(f2c);
    pipe(c2f);

    int pid = fork();

    if(pid == 0) {
        // child
        close(f2c[1]);
        close(c2f[0]);
        int cpid = getpid();
        fprintf(1, "%d: received ping\n", cpid);
        write(c2f[1], "pong", 4);
        close(c2f[1]);
    } else {
        // parent
        close(c2f[1]);
        close(f2c[0]);
        int fpid = getpid();
        write(f2c[1], "ping", 4);
        close(f2c[1]);
        wait(0);
        fprintf(1, "%d: received pong\n", fpid);
    }
    exit(0);
}