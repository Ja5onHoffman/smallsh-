#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>



void catchSIGINT(int signo)
{
    char* message = "SIGINT. Use CTRL-Z to Stop.\n";
    write(STDOUT_FILENO, message, 28);
}

char* removeNewline(char *str);
void execute(char** argv);

int main()
{
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    //SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

    int numCharsEntered = -5;       // How many chars we entered
    int currChar = -5;              // Tracks where we are when we print out every char
    size_t bufferSize = 0;          // Holds how large the allocated buffer is
    char* lineEntered = NULL;       // Points to a buffer allocated by getline() that holds our entered string + \n + \0
    char* parsedArgs[512];
    size_t MAX_LINE = 256;


    // // Init array of pointers to strings
    // for (int i = 0; i < MAX_LINE; i++) {
    //     parsedArgs[i] = (char*)malloc(MAX_LINE);
    // }

    while(1)
    {
        // Get input from the user
        while(1)
        {
            // Init array of pointers to strings
            for (int i = 0; i < MAX_LINE; i++) {
                parsedArgs[i] = (char*)malloc(MAX_LINE);
            }

            printf("smallsh: ");
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
                    //                    memcpy(&parsedArgs[c], tok, sizeof(tok) / sizeof(char));
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
        if (!strcmp(parsedArgs[0], "cd")) {
            char currentDir[1024];
            char *home = getenv("HOME");
            // If no args go to $HOME
            if (*parsedArgs[1] == '\0') {
                chdir(home);
                getcwd(currentDir, sizeof(currentDir));
                printf("%s\n", currentDir);
            } else {
                int r;
                removeNewline(parsedArgs[1]);
                r = chdir(parsedArgs[1]);
                // If successful
                if (r >=0) {
                    getcwd(currentDir, sizeof(currentDir));
                    printf("%s\n", currentDir);
                } else {
//                  printf("%d\n", strerr(errno));
                    printf("err\n");
                }
            }


        // MARK: status command
        } else if (!strcmp(parsedArgs[0], "status")) {

            // Built in commands do not have to support exit status



        // MARK: exit command
        } else if (!strcmp(parsedArgs[0], "exit")) {

          int childExitMethod;
          pid_t childPid = wait(NULL);e
          exit(0);

        } else {
            pid_t spawnPid = -5;
            int childExitStatus = -5;

            // Is this leaving a zombie
            spawnPid = fork();
            if (spawnPid == 0) {
              execute(parsedArgs);
            }

            // Wait to terminate
            pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
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



//
