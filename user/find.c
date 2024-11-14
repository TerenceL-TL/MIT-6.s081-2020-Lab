#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  memset(buf, 0, DIRSIZ+1);

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  return buf;
}

void find(char* path, char* name)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        printf("find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type)
    {
        case T_FILE:
            if(strcmp(fmtname(path), name) == 0)
            {
                printf("%s\n", path);
            }
            break;

        case T_DIR:
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("find: path too long\n");
                break;
                }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';
            // iterate and recursive find
            while(read(fd, &de, sizeof(de)) == sizeof(de)) {
                if(de.inum == 0 || !strcmp(de.name, ".") || !strcmp(de.name, ".."))
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0; // concat path/file
                if(stat(buf, &st) < 0){
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                find(buf, name);
            }
            break;
        default:
            break;
    }
    close(fd);
}


int 
main(int argc, char* argv[])
{
    if(argc != 3)
    {
        fprintf(2, "Usage: find <path> <name>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}