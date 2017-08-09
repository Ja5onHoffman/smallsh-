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
int bgMode = 1;

void sigquit_handler (int sig) {
    assert(sig == SIGQUIT);
    pid_t self = getpid();
    if (parent_pid != self) _exit(0);
}

void sigchld_handler(int sig) {

    // Sets bgQuit to 1 to flag that the bg process has completed
    bgQuit = 1;

}

void sigtstp_handler(int sig) {
    if (bgMode) {
        char *m = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, m, 49);
        bgMode = 0;
    } else {
        char *m = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, m, 29);
        bgMode = 1;
    }
}

// void sigterm_handler(int sig) {
//
//   bgQuit = 1;
// }

void getStatus(int *childExitMethod);
void rdbg(char **args, int len, int *io, int bgMode, int *bg, char *in, char *out);
void redirect(char *in, char *out, int io);
char* removeNewline(char *str);
void execute(char** argv);


int main()
{
  // Get pid of partent process
  parent_pid = getpid();

  // Registering signal handlers
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = catchSIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = SA_RESTART;
  sigaction(SIGINT, &SIGINT_action, NULL);

  struct sigaction SIGCHLD_action = {0};
  SIGCHLD_action.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &SIGCHLD_action, NULL);


  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = sigtstp_handler;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);


  int numCharsEntered = -5;       // How many chars we entered
  size_t bufferSize = 0;          // Holds how large the allocated buffer is
  char* lineEntered = NULL;       // Points to a buffer allocated by getline() that holds our entered string + \n + \0
  char* parsedArgs[512];
  size_t MAX_LINE = 256;          // Max line size
  pid_t bgPid = -5;               // Holds pid of background process
  int childExitMethod;
  int io = 0, bg = 0;             // Flags for redirection and background
  char in[256], out[256];         // Holds input and output files


    while(1) {
        while(1) {
            int c = 0;                    // Reset counter
            io = 0;                      // Reset io and bg
            // Reset file names
            memset(&in, 0, sizeof(in));
            memset(&out, 0, sizeof(out));

            // Init array of pointers to strings
            int i;
            for (i = 0; i < MAX_LINE; i++) {
                parsedArgs[i] = (char*)malloc(MAX_LINE);
            }

            // If the background process has quit and we're in background mode
            // clean up the process.

            if (bgQuit && bg) {
                waitpid(bgPid, &childExitMethod, WNOHANG);
                printf("background pid %d is done: ", bgPid);
                fflush(stdout);
                int exitStatus = WEXITSTATUS(childExitMethod);
                if (WIFEXITED(childExitMethod)) {
                    printf("exit value %d\n", exitStatus);
                } else {
                    printf("terminated by a signal %d\n", exitStatus);
                }
                fflush(stdout);

                // Reset bgquit and bg flags
                bgQuit = 0;
                bg = 0;
            }


            // Much of this taken from lectures
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
                // Loop to set pointers in parsedArgs
                while(tok) {
                    strcpy(parsedArgs[c], tok);
                    tok = strtok(NULL, " ");
                    c++;
                }
                // Remove newline on last input and set index after last to NULL for execvp
                removeNewline(parsedArgs[c-1]);
                parsedArgs[c] = (char*)0;

                // If first character is a comment do nothing
                // else parse the arguments in rdbg
                if (*parsedArgs[0] == '#') {
                    continue;
                } else {
                  // Parse the arguments
                  rdbg(parsedArgs, c, &io, bgMode, &bg, in, out);
                  break;
                }
            }
        }

        // Set spawnPid
        int spawnPid = -5;
        spawnPid = fork();

        // If child process perform action
        // Instructions say NOT to perform bulit in actions
        // child process
        if (spawnPid == 0) {

            // If io flag is set, enable redirection with redirect function
            if (io) {
                redirect(in, out, io);
            }

            // if (bg) {
            //   bgPid = getpid();
            // }

            // If cd
            if (!strcmp(parsedArgs[0], "cd")) {
                char currentDir[1024];
                char *home = getenv("HOME");
                // If no args go to $HOME
                if (!parsedArgs[1]) {
                    chdir(home);
                    getcwd(currentDir, sizeof(currentDir));
                } else {
                    // If more than one arg then go to designated directory
                    removeNewline(parsedArgs[1]);
                    chdir(parsedArgs[1]);
                    getcwd(currentDir, sizeof(currentDir));
                }

            // Status command
            } else if (!strcmp(parsedArgs[0], "status")) {

              // Use getStatus function for status printout
                getStatus(&childExitMethod);

            // IF exit
            } else if (!strcmp(parsedArgs[0], "exit")) {


              // Kill child process
              kill(spawnPid, SIGTERM);

            } else {
                // If not a built in command, use execute
                execute(parsedArgs);
            }
        } else {

            // If parent process block while child finishes
            if (!bg) {
                waitpid(spawnPid, &childExitMethod, 0);
                if (WIFSIGNALED(childExitMethod)) {
                    getStatus(&childExitMethod);
                }
            // Else return control to parent and keep track of background process pid
            } else {
                if (bg) {
                    bgPid = spawnPid;
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);
                }
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

// Redirect function takes input and output files and io flag
// to determine whether to set up for input, output or both.
// io flag is set in rdbg function
void redirect(char *in, char *out, int io) {
    int file_out = 0, file_in = 0;
    switch(io) {
      // Case 0 don't redirect
        case 0:
            break;
        // Case 1 open file for writing and redirect with dup2
        // Or exit if file doesn't open
        case 1:
            file_out = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            fcntl(file_out, F_SETFD, FD_CLOEXEC);
            if (file_out < 0) {
                exit(1);
            } else {
                dup2(file_out, STD_OUT);
            }
            break;
        // Case 2 open for reading
        case 2:
            file_in = open(in, O_RDONLY, 0644);
            fcntl(file_in, F_SETFD, FD_CLOEXEC);
            if (file_in < 0) {
                printf("cannot open file for input\n");
                // close(file_in);
                fflush(stdout);
                exit(1);
            } else {
                dup2(file_in, STD_IN);
            }
            break;
        // Case 3 open for reading and writing
        case 3:
            file_in = open(in, O_RDONLY, 0644);
            file_out = open(out, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG);

            // Need to handle error here as well
            dup2(file_in, STD_IN);
            dup2(file_out, STD_OUT);
            break;
    }
}

// Parses input to handle background and redirection
void rdbg(char **args, int len, int *io, int bgMode, int *bg, char *in, char *out) {
    int clean = 0, ig = 0;
    ig = (!strcmp(args[0], "status") || !strcmp(args[0], "exit") || !strcmp(args[0], "cd") || !bgMode);
    if (*args[len-1] == '&' && !ig) {
        *bg = 1; clean = 1;
        // i/o set to /dev/null in case no i/o inputs
        strcpy(in, "/dev/null");
        strcpy(out, "/dev/null");
    }

    if (ig && !bgMode) {
        clean = 1;
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
