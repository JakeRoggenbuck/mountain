#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void help() {
    print_and_exit("Usage:\n\
	mountain [OPTIONS] [FILE]\n\n\
META OPTIONS\n\
	-h, --help\t\tShow this page\n\
	-v, --version\t\tShow the version and exit\n\n\
DISPLAY OPTIONS\n\
	-V, --verbose\t\tVerbose output\n");
}

void version() { print_and_exit("mountain 0.0.1\n"); }

enum Option { HELP, VERSION, RUN };

struct Args {
    int verbose;
    char *filename;
    enum Option option;
};

void debug_args(struct Args *args) {
    printf("verbose: %d\n", args->verbose);
    printf("filename: %s\n", args->filename);
    printf("option: %d\n", args->option);
}

int does_match(char *in, char *short_, char *long_) {
    return (strcmp(in, short_) == 0) || (strcmp(in, long_) == 0);
}

struct Args parse(int argc, char **argv) {
    struct Args args;
    int has_file = 0;
    int has_option = 0;

    args.option = RUN;
    args.verbose = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!has_option) {
                if (does_match(argv[i], "-v", "--version")) {
                    args.option = VERSION;
                    has_option = 1;
                } else if (does_match(argv[i], "-h", "--help")) {
                    args.option = HELP;
                    has_option = 1;
                }
            }

            if (does_match(argv[i], "-V", "--verbose")) {
                args.verbose = 1;
            }
        } else {
            // Only accept the first non-flag as a file
            if (!has_file) {
                size_t len = strlen(argv[i]);
                args.filename = malloc(len * sizeof(char));
                strcpy(args.filename, argv[1]);
                has_file = 1;
            }
        }
    }

    if (args.filename == NULL || argc < 2) {
        help();
    }

    return args;
}

void on_create(struct inotify_event *event) {
    char buf[200];

    sprintf(buf, "notify-send \"New device %s!\"", event->name);
    system(buf);
}

void run(struct Args *args) {
    int length, i = 0, wd;
    int fd;
    char buffer[BUF_LEN];

    fd = inotify_init();
    if (fd < 0) {
        print_and_exit("Couldn't initialize inotify.\n");
    }

    wd = inotify_add_watch(fd, args->filename, IN_CREATE);

    if (args->verbose) {
        if (wd == -1) {
            printf("Couldn't add watch to \"%s\"\n", args->filename);
            exit(1);
        } else {
            printf("Watching \"%s\"\n", args->filename);
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
                    // Is a file being created
                    if (!(event->mask & IN_ISDIR)) {
                        if (args->verbose) {
                            printf("The file %s was created.\n", event->name);
                        }
                        on_create(event);
                    }
                }

                i += EVENT_SIZE + event->len;
            }
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}

int main(int argc, char **argv) {
    struct Args args = parse(argc, argv);

    switch (args.option) {
    case HELP:
        help();
        break;
    case VERSION:
        version();
        break;
    case RUN:
        run(&args);
        break;
    }

    return 0;
}
