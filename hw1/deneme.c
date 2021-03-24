
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define handle_error(msg) \
       do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct linux_dirent {
   long           d_ino;
   off_t          d_off;
   unsigned short d_reclen;
   char           d_name[];
};

#define BUF_SIZE (1024 * 1024 * 5)

void listdir(const char * dirname)
{
    int fd, nread;
    struct linux_dirent *d;
    int bpos;
    char d_type;
    char * buf = malloc(BUF_SIZE);
    struct stat fileStat;

    fd = open(dirname, O_RDONLY | O_DIRECTORY);
    if (fd == -1)
        handle_error("open");

    for ( ; ; ) {
        nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
        if (nread == -1)
            handle_error("getdents");

        if (nread == 0)
            break;

        for (bpos = 0; bpos < nread;) {
            d = (struct linux_dirent *) (buf + bpos);
            
            d_type = *(buf + bpos + d->d_reclen - 1);
            
            if(d->d_ino && strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
            {
                printf("%s/%s\n",dirname, d->d_name);
                int dirname_len = strlen(dirname);
                char * subdir = calloc(1, PATH_MAX + 1);
                strcat(subdir, dirname);
                strcat(subdir + dirname_len, "/");
                strcat(subdir + dirname_len + 1, d->d_name);
                printf("-%s\n",subdir );
                stat(subdir,&fileStat);
                if(S_ISDIR(fileStat.st_mode))
                {

                    listdir(subdir);
                    free(subdir);
                }
                else{
                    //printf("%s/%s\n",dirname, d->d_name);
                }
            }
            bpos += d->d_reclen;
            //break;
        }
    }

    close(fd);
    free(buf);
}

int
main(int argc, char *argv[])
{
    listdir("testfile");
    exit(EXIT_SUCCESS);
}