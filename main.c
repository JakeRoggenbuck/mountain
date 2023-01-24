#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_EVENTS 1024
#define LEN_NAME 16
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (MAX_EVENTS * (EVENT_SIZE + LEN_NAME))

void print_and_exit(const char *msg) {
    printf(msg);
    exit(1);
}

int main(int argc, char **argv) {
    int length, i = 0, wd;
    int fd;
    char buffer[BUF_LEN];
    char *filename = "/dev/";

    int verbose = 1;

    fd = inotify_init();
    if (fd < 0) {
        print_and_exit("Couldn't initialize inotify.\n");
    }

    wd = inotify_add_watch(fd, filename, IN_CREATE);

    if (verbose) {
        if (wd == -1) {
            printf("Couldn't add watch to \"%s\"\n", filename);
            exit(1);
        } else {
            printf("Watching \"%s\"\n", filename);
        }
    }

    while (1) {
        i = 0;
        length = read(fd, buffer, BUF_LEN);

        if (length < 0) {
            perror("read");
        }

        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    if (event->mask & IN_ISDIR) {
                        // printf("The directory %s was created.\n",
                        // event->name);
                    } else {
                        printf("The file %s was created.\n", event->name);
                    }
                }

                i += EVENT_SIZE + event->len;
            }
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);

    return 0;
}
