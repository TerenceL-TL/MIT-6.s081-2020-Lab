#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define ARGSIZ 64

int 
main(int argc, char* argv[])
{
    if(argc <= 1)
    {
        fprintf(2, "Usage: xargs [command [initial arguments]]\n");
        exit(1);
    }

    char c;
    char *pbuf;
    char *pbeg;
    char buf[(ARGSIZ + 1) * MAXARG];

    char **parg;
    char *exargv[MAXARG];
    pbuf = buf;
    pbeg = buf;
    parg = exargv;

    for(int i = 1; i < argc; i++)
    {
        *parg = argv[i];
        parg++;
    }

    while(read(0, &c, sizeof(c)))
    {
        if(c == '\n')
        {
            *pbuf++ = '\0';
            if (pbuf > pbeg) {
                *parg = pbeg;
                pbeg = pbuf;
                parg++;
            }
            pbuf = buf;
            // for(int i = 0; i < MAXARG && exargv[i]; i++) {
            //     printf("Argument %d: %s\n", i, exargv[i]);
            // }
            if(fork() == 0)
            {
                exec(exargv[0], exargv);
            }
            else
            {
                wait(0);
            }
            parg = &exargv[argc];
            continue;
        } 
        else if (c == ' ')
        {
            *pbuf++ = '\0';
            if (pbuf > pbeg) {
                *parg = pbeg;
                pbeg = pbuf;
                parg++;
            }
            pbuf = buf;
            continue;
        }
        
        if(pbuf >= pbeg + ARGSIZ)
        {
            fprintf(2, "Argument too long!\n");
            exit(1);
        }
        *pbuf++ = c;
    }

    exit(0);
}