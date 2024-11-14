#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXPN 36

int 
main(int argc, char* argv[])
{
    if(argc != 1)
    {
        fprintf(2, "Usage: primes\n");
        exit(1);
    }

    int num[MAXPN];
    for(int i = 0; i < MAXPN; i++)
    {
        num[i] = 0;
    }
    num[1] = num[0] = 1;

    int* pnum = num;

    while(1)
    {
        int p[2];
        pipe(p);
        
        int l_nb = 0;
        for(int i = 0; i < MAXPN; i++)
        {
            if(pnum[i] == 0)
            {
                l_nb = i;
                break;
            }
        }// find left neighbor

        if(l_nb == 0)
        {
            break;
        }

        fprintf(1, "prime %d\n", l_nb);

        int value = 0;
        if(fork() == 0)
        {
            close(p[0]);
            for(int idx = 1; idx * l_nb < MAXPN; idx++)
            {
                value = idx * l_nb;
                write(p[1], &value, sizeof(value));
            }
            close(p[1]);
            exit(0);
        } else {
            close(p[1]);
            while(read(p[0], &value, sizeof(value)) != 0)
            {
                pnum[value] = 1;
            }
            close(p[0]);
        }
    }
    exit(0);
}