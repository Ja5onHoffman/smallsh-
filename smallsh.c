#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#define STD_IN  0
#define STD_OUT 1
#define STD_ERR 2

void catchSIGINT(int signo)
{
    // char* message = "SIGINT. Use CTRL-Z to Stop.\n";
    // write(STDOUT_FILENO, message, 28);
}

pid_t parent_pid;
int bg = 0;
int bgQuit = 0;

void sigquit_handler (int sig) {
    assert(sig == SIGQUIT);
    pid_t self = getpid();
    // printf("pid: %d\n", self);
    if (parent_pid != self) _exit(0);
}

void sigchld_handler(int sig) {
    // int status;
    // int saved_errno = errno;
    // waitpid(-1, &status, WNOHANG) > 0) {}
    bgQuit = 1;
    // errno = saved_errno;
    // write(STDOUT_FILENO, &bg, 2);
    // if (bg) {
    //   char *backgroundpid = "background pid";
    //   char *isdone = " is done: exit value";
    //   pid_t p = getpid();
    //   int s = WIFEXITED(status);
    //   write(STDOUT_FILENO, backgroundpid,14);
    //   write(STDOUT_FILENO, &p, sizeof(p));
    //   write(STDOUT_FILENO, isdone, 20);
    //   write(STDOUT_FILENO, &s, sizeof(s));
    // }
}

void getStatus(int *childExitMethod);
void rdbg(char **args, int len, int *io, int *bg, char *in, char *out);
void redirect(char *in, char *out, int io);
char* removeNewline(char *str);
void execute(char** argv);
int waitbg(int childPid, int *childExitMethod);
void traceParsedArgs(char** args);


int main()
{

    parent_pid = getpid();
    // struct sigaction SIGINT_ignore = {0};
    // SIGINT_ignore.sa_handler = SIG_IGN;
    // sigaction(SIGINT, &SIGINT_ignore, NULL);
    //
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

    struct sigaction SIGCHLD_action = {0};
    SIGCHLD_action.sa_handler = &sigchld_handler;
    sigaction(SIGCHLD, &SIGCHLD_action, NULL);


    // struct sigaction SIGQUIT_action = {0};
    // SIGQUIT_action.sa_handler = sigquit_handler;
    // sigfillset(&SIGQUIT_action.sa_mask);
    // sigaction(SIGQUIT, &SIGINT_action, NULL);


    int numCharsEntered = -5;       // How many chars we entered
    int currChar = -5;              // Tracks where we are when we print out every char
    size_t bufferSize = 0;          // Holds how large the allocated buffer is
    char* lineEntered = NULL;       // Points to a buffer allocated by getline() that holds our entered string + \n + \0
    char* parsedArgs[512];
    size_t MAX_LINE = 256;
    pid_t topPid = getpid();
    pid_t childPid;
    pid_t bgPid = -5;
    int childExitMethod;
    int io = 0, bg = 0; // Flags for redirection and background
    char in[256], out[256];

    // // Init array of pointers to strings
    // for (int i = 0; i < MAX_LINE; i++) {
    //     parsedArgs[i] = (char*)malloc(MAX_LINE);
    // }

    while(1) {
        while(1) {
            int c = 0;                    // Reset counter
            io = 0;                       // Reset io and bg
            memset(&in, 0, sizeof(in));   // Reset file descriptors
            memset(&out, 0, sizeof(out));

            // Init array of pointers to strings
            int i;
            for (i = 0; i < MAX_LINE; i++) {
                parsedArgs[i] = (char*)malloc(MAX_LINE);
            }

            if (bgQuit && bg) {
                // Clean up bg process
                waitpid(bgPid, &childExitMethod, 0);
                printf("background pid %d is done: ", bgPid);
                fflush(stdout);
                int exitStatus = WEXITSTATUS(childExitMethod);
                if (WIFEXITED(childExitMethod)) {
                    printf("exit value %d\n", exitStatus);
                } else {
                    printf("terminated by a signal %d\n", exitStatus);
                }
                fflush(stdout);
                // Reset bgquit and bg
                bgQuit = 0;
                bg = 0;
            }



            printf(": ");
            // Get a line from the user
            fflush(stdin);
            fflush(stdout);
            numCharsEntered = getline(&lineEntered, &bufferSize, stdin);
            if (numCharsEntered == -1) {
                clearerr(stdin);
            } else if (numCharsEntered > 2048) {
                printf("Max characters exceeded\n");
            } else {
                char *tok = strtok(lineEntered, " ");
                while(tok) {
                    // memcpy(&parsedArgs[c], tok, sizeof(tok) / sizeof(char));
                    strcpy(parsedArgs[c], tok);
                    tok = strtok(NULL, " ");
                    c++;
                }
                // Remove newline on last input and set index after last to NULL for exec
                removeNewline(parsedArgs[c-1]);
                parsedArgs[c] = (char*)0;
                // Loop everything if comment
                // if (!strcmp(*parsedArgs, "#") || !strcmp(&parsedArgs[0][0], "#")) {
                if (*parsedArgs[0] == '#') {
                    continue;
                } else {
                    rdbg(parsedArgs, c, &io, &bg, in, out);
                    break;
                }
            }
            // rdbg(parsedArgs, c, &io, &bg, in, out);
        }

        int spawnPid = -5;
        spawnPid = fork();

        if (spawnPid == 0) {
            if (io) {
                redirect(in, out, io);
            }

            // if (bg) {
            //   bgPid = getpid();
            // }

            if (!strcmp(parsedArgs[0], "cd")) {
                char currentDir[1024];
                char *home = getenv("HOME");
                // If no args go to $HOME
                if (!parsedArgs[1]) {
                    chdir(home);
                    getcwd(currentDir, sizeof(currentDir));
                    printf("%s\n", currentDir);
                    fflush(stdout);
                } else {
                    int r;
                    removeNewline(parsedArgs[1]);
                    chdir(parsedArgs[1]);
                    getcwd(currentDir, sizeof(currentDir));
                    printf("%s\n", currentDir);
                    fflush(stdout);
                }

                // MARK: status command
                // This one does't run in background
            } else if (!strcmp(parsedArgs[0], "status")) {
                getStatus(&childExitMethod);
                // MARK: exit command
            } else if (!strcmp(parsedArgs[0], "exit")) {
                // int t = 0;
                // while (!t) {
                //   t = waitpid(-1, &childExitMethod, WNOHANG);
                // }
                // kill(parent_pid, SIGTERM);
                raise(SIGTERM);

            } else {
                execute(parsedArgs);
            }
        } else {
            // If not a background process block
            if (!bg) {
                waitpid(spawnPid, &childExitMethod, 0);
                if (WIFSIGNALED(childExitMethod)) {
                    getStatus(&childExitMethod);
                }
            } else {
                if (bg) {
                    bgPid = spawnPid;
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);
                }
                // waitbg(spawnPid, &childExitMethod);
            }
        }
    }

    return 0;
}

// Execute args (taken from lecture slide)
void execute(char** argv) {
    if (execvp(*argv, argv) < 0) {
        printf("execvp failed\n");
        exit(1);
    }
}

// Helper function to remove a new line from character strings
char* removeNewline(char *str) {
    // Search string for newline character
    char *p = strchr(str, '\n');
    // If found change to a terminator
    if (p != NULL) *p = '\0';
    return str;
}

void getStatus(int *childExitMethod) {
    int exitStatus;
    if (WIFEXITED(*childExitMethod)) {
        exitStatus = WEXITSTATUS(*childExitMethod);
        printf("exit value %d\n", exitStatus);
        fflush(stdout);
    } else if (WIFSIGNALED(*childExitMethod)) {
        printf("terminated by signal %d\n", WTERMSIG(*childExitMethod));
        fflush(stdout);
    }
}

void redirect(char *in, char *out, int io) {
    int file_out = 0, file_in = 0;
    int res;
    switch(io) {
        case 0:
            break;
        case 1:
            file_out = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            fcntl(file_out, F_SETFD, FD_CLOEXEC);
            if (file_out < 0) {
                return;
            } else {
                dup2(file_out, STD_OUT);
            }
            break;
        case 2:
            file_in = open(in, O_RDONLY, 0644);
            fcntl(file_in, F_SETFD, FD_CLOEXEC);
            if (file_in < 0) {
                printf("cannot open file for input\n");
                // close(file_in);
                fflush(stdout);
                return;
            } else {
                dup2(file_in, STD_IN);
            }
            break;
        case 3:
            file_in = open(in, O_RDONLY, 0644);
            file_out = open(out, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG);

            // Need to handle error here as well
            dup2(file_in, STD_IN);
            dup2(file_out, STD_OUT);
            break;
    }
}

int waitbg(int childPid, int *childExitMethod) {
    int b = waitpid(childPid, childExitMethod, WNOHANG);
    return b;
}


// Parses input to handle background and redirection
void rdbg(char **args, int len, int *io, int *bg, char *in, char *out) {
    int clean = 0, ig = 0;
    ig = (!strcmp(args[0], "status") || !strcmp(args[0], "exit") || !strcmp(args[0], "cd"));
    if (*args[len-1] == '&' && !ig) {
        *bg = 1; clean = 1;
        // i/o set to /dev/null in case no i/o inputs
        strcpy(in, "/dev/null");
        strcpy(out, "/dev/null");
    }

    int i;
    for (i = 0; i < len; i++) {
        if (*args[i] == '>') {
            *io = 1; clean = 1;
            strcpy(out, args[i+1]);
        } else if (*args[i] == '<') {
            *io = 2; clean = 1;
            strcpy(in, args[i+1]);
        }
    }
    // If input and output io flag = 3;
    if (*out && *in) { *io = 3; };

    if (clean) {
        int i;
        for (i = 0; i < len; i++) {
            char c = *args[i];
            if (c == '>' || c == '<' || c == '&') {
                for (; i < len; i++) {
                    args[i] = (char*)0;
                }
                break;
            }
        }
    }
}

void traceParsedArgs(char** args) {
    int i;
    for (i = 0; i < 3; i++) {
        printf("%s\n", args[i]);
    }
}
