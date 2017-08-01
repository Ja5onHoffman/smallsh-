#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <assert.h>



void catchSIGINT(int signo)
{
    char* message = "SIGINT. Use CTRL-Z to Stop.\n";
    write(STDOUT_FILENO, message, 28);
}

pid_t parent_pid;

void sigquit_handler (int sig) {
    assert(sig == SIGQUIT);
    pid_t self = getpid();
    printf("pid: %d\n", self);
    if (parent_pid != self) _exit(0);
}

void getStatus(int *childExitMethod);


char* removeNewline(char *str);
void execute(char** argv);

int main()
{
    // struct sigaction SIGINT_ignore = {0};
    // SIGINT_ignore.sa_handler = SIG_IGN;
    // sigaction(SIGINT, &SIGINT_ignore, NULL);
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

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
    int childExitMethod;
    // // Init array of pointers to strings
    // for (int i = 0; i < MAX_LINE; i++) {
    //     parsedArgs[i] = (char*)malloc(MAX_LINE);
    // }

    while(1) {
        // Get input from the user
        while(1) {
            // Init array of pointers to strings
            for (int i = 0; i < MAX_LINE; i++) {
                parsedArgs[i] = (char*)malloc(MAX_LINE);
            }

            printf(": ");
            // Get a line from the user
            fflush(stdin);
            numCharsEntered = getline(&lineEntered, &bufferSize, stdin);
            if (numCharsEntered == -1) {
                clearerr(stdin);
            } else if (numCharsEntered > 2048) {
                printf("Max characters exceeded\n");
            } else {
                int c = 0;
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
                if (!strcmp(parsedArgs[0], "#") || !strcmp(&parsedArgs[0][0], "#")) {
                    continue;
                } else {
                    break;
                }
            }
        }

        // MARK: cd command
        if (strcmp(parsedArgs[0], "cd") == 0) {
          pid_t spawnPid = -5;
          // int childExitMethod;
          spawnPid = fork();
          // If child run cd
          if (spawnPid == 0) {
            char currentDir[1024];
            char *home = getenv("HOME");
            // If no args go to $HOME
            if (!parsedArgs[1]) {
                chdir(home);
                getcwd(currentDir, sizeof(currentDir));
                printf("%s\n", currentDir);
            } else {
                int r;
                removeNewline(parsedArgs[1]);
                if (!chdir(parsedArgs[1])) {
                  exit(1);
                }
            }
            exit(0); // Successful exit

          // Else wait for cd to run
          } else {
            waitpid(spawnPid, &childExitMethod, 0);
          }


        // MARK: status command
        // This one does't run in background
        } else if (!strcmp(parsedArgs[0], "status")) {
          getStatus(&childExitMethod);

        // MARK: exit command
        } else if (!strcmp(parsedArgs[0], "exit")) {

        } else {
            pid_t spawnPid = -5;
            spawnPid = fork();
            if (spawnPid == 0) {
              execute(parsedArgs);
            } else {
                waitpid(spawnPid, &childExitMethod, 0);
                if (WIFSIGNALED(childExitMethod)) {
                  getStatus(&childExitMethod);
                }

            }
        }
    }

// TAG: atexit() can clean up the child processes


    // printf("Allocated %zu bytes for the %d chars you entered.\n",
    //         bufferSize, numCharsEntered);
    // printf("Here is the raw entered line: \"%s\"\n", lineEntered);
    // // Remove the trailing \n that getline adds
    // lineEntered[strcspn(lineEntered, "\n")] = '\0';
    // printf("Here is the cleaned line: \"%s\"\n", lineEntered);
    // // Free the memory allocated by getline() or else memory leak
    // free(lineEntered);
    // lineEntered = NULL;
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
  } else if (WIFSIGNALED(*childExitMethod)) {
    printf("terminated by signal %d\n", WTERMSIG(*childExitMethod));
  }
}


//
