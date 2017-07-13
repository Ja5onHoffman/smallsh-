#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main()
{
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = catchSIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  //SIGINT_action.sa_flags = SA_RESTART;
  sigaction(SIGINT, &SIGINT_action, NULL);

  int numCharsEntered = -5; // How many chars we entered
  int currChar = -5; // Tracks where we are when we print out every char
  size_t bufferSize = 0; // Holds how large the allocated buffer is
  char* lineEntered = NULL; // Points to a buffer allocated by getline() that holds our entered string + \n + \0

  while(1)
  {
    // Get input from the user
    while(1)
    {
      printf("smallsh: ");
      // Get a line from the user
      numCharsEntered = getline(&lineEntered, &bufferSize, stdin);
      if (numCharsEntered == -1)
        clearerr(stdin);
      else
        break; // Exit the loop - we've got input



      }

      lineEntered[strcspn(lineEntered, "\n")] = '\0';
      if (!strcmp(lineEntered, "exit")) {
        printf("exit\n");
      } else if (!strcmp(lineEntered, "cd")) {
        printf("cd\n");
      } else if (!strcmp(lineEntered, "status")) {
        printf("status\n");
      } else {
        printf("huh?");
      }
    }

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
