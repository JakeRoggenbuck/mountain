#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/stat.h>
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
	-V, --verbose\t\tVerbose output\n\n\
FEATURE OPTIONS\n\
	-m, --mount\t\tAutomatically mount drives when found\n");
}

void version() { print_and_exit("mountain 0.0.1\n"); }

enum Option { HELP, VERSION, RUN };

struct Args {
    int verbose;
    char *filename;
    enum Option option;
    int mount;
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
    args.mount = 0;

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

            if (does_match(argv[i], "-m", "--mount")) {
                args.mount = 1;
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

    if (args.mount) {
        if (geteuid() != 0) {
            print_and_exit(
                "Permission Error: mountain needs root to mount drives.\n");
        }
    }

    return args;
}

void on_create(struct Args *args, struct inotify_event *event) {
    char foundfile[200];
    char newfile[200];
    int did_make_dir;
    struct stat st = {0};

    // Create filepath of device
    sprintf(foundfile, "%s/%s", args->filename, event->name);

    // Create mounting point
    sprintf(newfile, "/tmp/%s-mountain", event->name);

    if (args->mount) {
        // Check that it's a drive - TODO: temporary
        if (event->name[0] == 's' && event->name[1] == 'd') {

            // Check if directory already exists
            if (stat(newfile, &st) == -1) {
                did_make_dir = (mkdir(newfile, 0700) == 0);

                if (args->verbose) {
                    if (did_make_dir) {
                        printf("Created new directory: %s.\n", newfile);
                    } else {
                        printf("Error creating directory: %s.\n", newfile);
                    }
                }
            }

            // Mount the drive
            printf("Attempting: %s -> %s\n", foundfile, newfile);
            if (mount(foundfile, newfile, "vfat", MS_NOATIME, NULL)) {
                if (errno == EBUSY) {
                    printf("Mount error: mountpoint busy.\n");
                } else if (errno == EPERM) {
                    printf("Mount error: you cannot perform this operation "
                           "unless you are root.\n");
                } else {
                    printf("Mount error: %s.\n", strerror(errno));
                    if (args->verbose) {
                        printf("Mount error number: %d.\n", errno);
                    }
                }
            } else {
                printf("Mount successful.\n");
                printf("Finished: %s -> %s\n", foundfile, newfile);
            }
        }
    } else {
        if (args->verbose) {
            printf("Info: Found drive, but mount is off.\n");
        }
    }
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
                        on_create(args, event);
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
