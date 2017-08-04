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
void rdbg(char **args, int len, int *io, int *bg, char *in, char *out);
void redirect(char *in, char *out, int io);
char* removeNewline(char *str);
void execute(char** argv);

void traceParsedArgs(char** args);

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
    int io, bg; // Flags for redirection and background
    char in[256], out[256];
    // // Init array of pointers to strings
    // for (int i = 0; i < MAX_LINE; i++) {
    //     parsedArgs[i] = (char*)malloc(MAX_LINE);
    // }

    while(1) {
        // Get input from the user
        while(1) {
            int c = 0;            // Reset counter
            io = 0; bg = 0;   // Reset io and bg
            memset(&in, 0, sizeof(in));  // Reset file descriptors
            memset(&out, 0, sizeof(out));
            // Init array of pointers to strings
            for (int i = 0; i < MAX_LINE; i++) {
                parsedArgs[i] = (char*)malloc(MAX_LINE);
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
                if (!strcmp(parsedArgs[0], "#") || !strcmp(&parsedArgs[0][0], "#")) {
                    continue;
                } else {
                  rdbg(parsedArgs, c, &io, &bg, in, out);
                  break;
                }
            }
            rdbg(parsedArgs, c, &io, &bg, in, out);
        }

        int spawnPid = -5;
        spawnPid = fork();
        if (spawnPid == 0) {
          if (io) {
            redirect(in, out, io);
          }
          // traceParsedArgs(parsedArgs);
          if (strcmp(parsedArgs[0], "cd") == 0) {
            // If child run cd
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

          // MARK: status command
          // This one does't run in background
          } else if (!strcmp(parsedArgs[0], "status")) {
            getStatus(&childExitMethod);
            exit(0);
          // MARK: exit command
          } else if (!strcmp(parsedArgs[0], "exit")) {

          } else {
              execute(parsedArgs);

          }

          exit(0);
        } else {
          // If not a background process block
          if (!bg) {
            waitpid(spawnPid, &childExitMethod, 0);
            if (WIFSIGNALED(childExitMethod)) {
              getStatus(&childExitMethod);
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
  } else if (WIFSIGNALED(*childExitMethod)) {
    printf("terminated by signal %d\n", WTERMSIG(*childExitMethod));
  }
}

void redirect(char *in, char *out, int io) {
  int file_out, file_in;
  int res;
  switch(io) {
    case 0:
      break;
    case 1:
      file_out = open(out, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
      dup2(file_out, STD_OUT);
    case 2:
      file_in = open(out, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR);
      dup2(file_in, STD_IN);
  }
}


// Parses input to handle background and redirection
void rdbg(char **args, int len, int *io, int *bg, char *in, char *out) {
  int clean = 0;
  if (*args[len-1] == '&') {
        *bg = 1; clean = 1;
  }
  for (int i = 0; i < len; i++) {
    if (*args[i] == '>') {
      *io = 1; clean = 1;
      strcpy(out, args[i+1]);
    } else if (*args[i] == '<') {
      *io = 2; clean = 1;
      strcpy(in,  args[i+1]);
    }
  }

  // If input and output io flag = 3;
  if (*out && *in) { *io = 3; };

  if (clean) {
    for (int i = 0; i < len; i++) {
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
  for (int i = 0; i < 3; i++) {
    printf("%s\n", args[i]);
  }
}
