/* 
* Timothy Lexa - CS 344 smallsh
* All printed statements/error messages come from the provided sample
* program execution on Canvas. Unless another source is noted, the 
* Canvas explorations served as the main inspiration for all functions
* below, with some code snippets directly taken from there, along with
* general googling for smaller issues, mostly related to pointer 
* referencing and general C syntax
*
* Style of code and comments from: 
* https://www.cs.swarthmore.edu/~newhall/unixhelp/c_codestyle.html
*/

#define _POSIX_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>


// Defining constants and some global variables used in signal handlers
#define MAX_CHARACTERS 2048 
#define MAX_ARGS 512
int background_permission = 1;
int no_signal = 1;

/* FUNCTION: input
 *
 * PARAMETERS:
 *    arr[] = the array in which parsed arguments will be held
 *    process_id = smallsh's process ID
 *    background = variable that indicates if current command will be in background
 *
 * USE:
 *    This function obtains user input from the command line.
 *    First prints the prompt, then starts reading from stdin.
 *    Uses fgetc() to read one character at a time, performing $$ expansion, where
 *    any instance of "$$" is replaced by smallsh's process ID. Once \n is found, 
 *    reading stops and the full string is null terminated.
 *    Then uses strtok to split the string into individual arguments, using " " as
 *    the delimiter. Saves file names for redirection later on if < or > are used.
 *    Each token is added to the array of arguments as its own string.
 *    Finally checks if "&" is the last argument, in which case the 'background'
 *    variable is changed to indicate the command should be executed in background
 */
void input(char *arr[], int process_id, int *background, char input_name[], char output_name[])
{
  // Converting process id into a string 
  char pid[20];
  sprintf(pid, "%d", process_id);

  // Creating char to store command line input
  char user_input[MAX_CHARACTERS] = {0};

  // Printing prompt
  printf(": ");
  fflush(stdout);

  // Char to store input after $$ expansion
  char processed_input[MAX_CHARACTERS] = {0};

  // Variables to be used during processing
  int char_count = 0;
  int char1, char2;

  // Reads one character at a time, checking for various triggers, and processes
  // the character appropriately.
  while(no_signal){
    char1 = fgetc(stdin); // Grab next character
    // If character is newline, null terminate the string and stop processing
    if (char1 == 10){
      processed_input[char_count] = '\0';
      break;
    }
    // If character is $
    if (char1 == 36){
      char2 = fgetc(stdin);
      // Check if next character is also $, in which case we add 
      // the PID to the string, and adjust char_count for length of PID
      if (char2 == 36){
        strcat(processed_input, pid);
        char_count += strlen(pid);
      }
      // Covers case where $ is last character before newline. 
      // Adds the $ to string, null terminates string, and stops processing
      else if (char2 == 10){
        processed_input[char_count] = char1;
        char_count += 1;
        processed_input[char_count] = '\0';
        break;
      // If char1 is $ and char2 is any other character besides $ or newline. 
      // Add both to string, and adjust char_count for next loop
      }else{
        processed_input[char_count] = char1;
        processed_input[char_count+1] = char2;
        char_count += 2;
      }
    // If not $ or newline, add character to string, 
    // and adjust char_count for next loop
    } else{
      processed_input[char_count] = char1;
      char_count += 1;
    }
  }
  // No signal alerts if ^Z has been entered. If so, we dont take it as input,
  // and will just get input on next input loop
  if (no_signal){
    // Uses strtok to separate the processed input using " " as the delimiter. 
    // Uses word_index to keep track of spot in the arguments array. The tokens 
    // represent individual arguments, and are added to the array one by one. 
    int word_index = 0;
    char *token = strtok(processed_input, " ");
    while (token != NULL){
      // If < is by itself, the next token is the input file, so we save the name
      // We do not add the < or file name to our arguments array
      // Call strtok to get the next token and continue to next loop
      if (strcmp(token, "<") == 0){
        token = strtok(NULL, " ");
        strcpy(input_name, token);
        token = strtok(NULL, " ");
        continue;
      }
      // If > is by itself, the next token is the output file, so we save the name
      // We do not add the > or file name to our arguments array
      // Call strtok to get the next token and continue to next loop
      if (strcmp(token, ">") == 0){
        token = strtok(NULL, " ");
        strcpy(output_name, token);
        token = strtok(NULL, " ");
        continue;
      } else{ // If we dont have < or >, add the argument to the array and move on
        arr[word_index] = token;
        word_index += 1;
      }
      token = strtok(NULL, " ");
    }
    // Checking if & is the last argument. If so, change boolean to indicate that 
    // this command is to be executed in the background. Will check if we are in
    // foreground only mode later during execution. Also removes the "&" from the
    // arguments array, as it has served its purpose by changing background status
    if (arr[word_index - 1] != NULL && strcmp(arr[word_index - 1], "&") == 0){
      *background = 1;
      arr[word_index - 1] = NULL;
    }
  }
}

/* FUNCTION: reset_array
 *
 * PARAMETERS:
 *    arr[] = the array in which parsed arguments are held
 *
 * USE:
 *    This function takes in the array of pointers holding the 
 *    user's input arguments, and resets the array to prepare for
 *    the next input
 */
void reset_array(char *arr[]){
    int reset_index = 0;
    while (arr[reset_index] != NULL){
      arr[reset_index] = 0;
      reset_index++;
    }
}

/* FUNCTION: exit_kill
 *
 * PARAMETERS:
 *    backgroundPIDs[] = the array in which background PIDs are held
 * 
 * USE:
 *    This function takes in the array of integers that are the PIDs of 
 *    background processes started by smallsh. When user enters the exit 
 *    command, this function goes through the list of PIDs, calling kill() 
 *    on each. If a process had already terminated normally, calling kill()
 *    on it will have no adverse effect. 
 */
void exit_kill(int backgroundPIDs[]){
  int i;
  for (i =0;i < (sizeof (backgroundPIDs) /sizeof (backgroundPIDs[0]));i++) {
    if (backgroundPIDs[i] != 0){
      int result = kill(backgroundPIDs[i], SIGTERM);
      backgroundPIDs[i] = 0;
    }
  }
}


/* FUNCTION: SIGTSTP_handler
 *
 * USE:
 *    Handles the SIGTSTP signal (^Z)
 *    Uses the global variable background_permission to change whether we are 
 *    in foreground-only mode or not. Prints a message to let the user know which mode
 *    they've entered. Also sets no_signal to 0 so that input() doesn't get crossed up
 */
void SIGTSTP_handler (int signo){
  if (background_permission == 1){
    char *foreground_message = "\nEntering foreground-only mode (& is now ignored)\n";
    write(STDOUT_FILENO, foreground_message, 50);
    fflush(stdout);
    background_permission = 0;
  } else{
    char *background_message = "\nExiting foreground-only mode\n";
    write(STDOUT_FILENO, background_message, 30);
    fflush(stdout);
    background_permission = 1;
  }
  no_signal = 0;
}

/* FUNCTION: command_execution
 *
 * PARAMETERS:
 *    *arguments[] = the array in which parsed arguments are held
 *    *process_id = pointer to exit_status variable 
 *    command_background = variable that indicates if current command will be in background
 *    backgroundPIDs[] = array that holds the PIDs of started background processes 
 *    background_index = the current index of backgroundPIDS[] that a new background PID will be inserted into
 *    sigaction crtl_c = the struct holding our signal handling for SIGINT
 *    input_name[] = string holding the name of the input file for redirection
 *    output_name[] = string holding the name of the output file for redirection
 *
 * USE:
 *    This function handles a command that is not one of the 3 built-ins. 
 *    First, calls fork to start a child process. 
 *    Based on the result of fork, we move into command execution
 *    With successfuk fork, check if command is fore- or background
 *    Change our SIGINT handler to the default action if foreground
 *    Perform redirection for input and/or output
 *    Attempt to execute the command, exiting if there's an error
 *    Using waitpid() to block the parent if child is foreground, or
 *    no blocking if child is in background
 *    Finally, a loop to continously check for background processes that have 
 *    finished, and reporting to the user if there are new terminated PIDs
 *    Code snippets and outline taken from the Module 4 explorations regarding
 *    Process API, along with Prof. Brewster's "3.1 Processes" Lecture video on youtube:
 *    https://www.youtube.com/watch?v=1R9h-H2UnLs&t=534s
 */
void command_execution(char *arguments[], int *exit_status, int command_background,
    int backgroundPIDs[], int *background_index, struct sigaction crtl_c, char input_name[], char output_name[]){
  pid_t spawnPID = -5;
  int exec_result = 0;
  spawnPID = fork();
  // Uses switch statement to evaluate result of the fork
  // Structure pretty much ripped from "Process API – Creating and Terminating Processes" 
  // exploration on Canvas
  switch(spawnPID){ 
    case -1 :; 
      perror("fork() failed");
      exit(1);
      break;
    case 0 :; 
      // If the command is to be executed in the foreground, we switch our SIGINT handler
      // to handle SIGINT in the default method, which is to stop execution
      if (background_permission != 1 || command_background != 1){
        crtl_c.sa_handler = SIG_DFL;
        sigaction(SIGINT, &crtl_c, NULL);
      }
      // Redirection format taken from "Processes and I/O" Canvas exploration
      // Input redirection
      if (strcmp(input_name, "") != 0){  // Won't execute if no input redirection was requested by user
        int input_file = open(input_name, O_RDONLY); 
        // Error handling in the event that file couldn't be opened
        if (input_file== -1){
          printf("cannot open %s for input\n", input_name);
          fflush(stdout);
          *exit_status = 1;
          exit(1);
        }
        // Redirects input using dup2. '0' refers to stdin's file descriptor
        int input_redirect = dup2(input_file, 0);
        // Handling error if redirect wasn't successful
        if (input_redirect == -1){
          printf("could not redirect %s\n", input_name);
          fflush(stdout);
          *exit_status = 1;
          exit(1);
        }
        // Closing file
        fcntl(input_file, F_SETFD, FD_CLOEXEC);
      }
      // Output redirection
      if (strcmp(output_name, "") != 0){ // Won't execute if no output redirection was requested by user
        int output_file = open(output_name, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        // Error handling in the event that file couldn't be opened/created
        if (output_file == -1){
          printf("cannot open %s for output\n", output_name);
          fflush(stdout);
          *exit_status = 1;
          exit(1);
        }
        // Redirects output using dup2. '1' refers to stdout's file descriptor
        int output_redirect = dup2(output_file, 1);
        // Handling error if redirect wasn't successful
        if (output_redirect == -1){
          printf("could not redirect %s\n", output_name);
          fflush(stdout);
          *exit_status = 1;
          exit(1);
        }
        // Closing file
        fcntl(output_file, F_SETFD, FD_CLOEXEC);
      }
      int exec_result;
      exec_result = execvp(arguments[0], arguments);
      if (exec_result < 0){
        printf("%s: no such file or directory\n", arguments[0]);
        fflush(stdout);
        *exit_status = 1;
        exit(1);
      }
      break;
    default:; // This block handles the blocking/waiting for child processes
      // If command is supposed to be in the background, and we are not in foreground-only mode
      if(background_permission == 1 && command_background == 1){ 
        // Call waitpid with WNOHANG to allow the parent to continue on without waiting for the child to finish
        waitpid(spawnPID, exit_status, WNOHANG);
        // Adding the child's PID to the array of background PIDs, and incrementing index for next time
        backgroundPIDs[*background_index] = spawnPID;
        *background_index += 1;
        // Printing message to user with the background PID
        printf("background pid is %d\n", spawnPID);
        fflush(stdout);
      } else{ // Foreground commands
        // Calls waitpid, which blocks the parent process from continuing until the child is finished
        waitpid(spawnPID, exit_status, 0);
        // This if statement checks if the process ended by signal interruption
        if (WIFSIGNALED(*exit_status)){
          // prints a message indicating which signal caused the process to terminate
          printf("terminated by signal %d\n", WTERMSIG(*exit_status));
          fflush(stdout);
        } else {
          // If the child ended by normal means, we set the exit_status variable to equal the
          // exit code, which would be returned by the status built in
          *exit_status = WEXITSTATUS(*exit_status);
        }
      }
    // This section checks for completed background processes, and prints a message
    // once one has completed. -1 indicates 'waiting' for any process that has changed state,
    // which in this case would indicate the completion of the process, returning that PID
    // Code inspired by 'Monitoring Child Processes' Canvas
    // exploration and https://stackoverflow.com/a/14548561
    int terminated_pid = 0;
    while ((terminated_pid = waitpid(-1, exit_status, WNOHANG)) > 0) { 
      printf("background pid %d is done: ", terminated_pid);
      fflush(stdout);
      // If process exited normally, print exit value
      if (WIFEXITED(*exit_status)){ 
        printf("exit value %d\n", WEXITSTATUS(*exit_status));
        fflush(stdout);
      } else {
        // If process ended as a result of signal interruption, print which signal
        printf("terminated by signal %d\n", WTERMSIG(*exit_status)); 
        fflush(stdout);                             
      }
    }
  }
}

/*
 * FUNCTION: main
 *
 * USE: 
 *    Controls the program. First initializes variables to be used
 *    throughout the program. Also creates sigaction structs to handle
 *    the various signals. A while loop then controls the flow of 
 *    the program. First calls input() to obtain and process user input.
 *    Has blocks that handle blank input, along with the three built-in
 *    commands (cd, exit, and status). If a different command is indicated,
 *    calls command_execution() to handle it. While loop continues until
 *    user indicates it should end with the exit command
 */
int main()
{
  // Saving process ID as string, and initializing permission to start background processes
  int process_id = getpid();


  // Array to hold strings containing max of 512 arguments
  char *arguments[MAX_ARGS];

  // Initializing exit_status
  int exit_status = 0;

  // Array to hold PIDs of running background processes. Index counter to keep track of where to insert
  int backgroundPIDs[200] = {0};
  int background_index = 0;
  
  // Creates struct for SIGTSTP (^Z) Sets the handler as the previously defined SIGTSTP_handler,
  // which will be called whenever SIGTSTP is caught
  struct sigaction sigtstp = {0};
  sigtstp.sa_handler = SIGTSTP_handler;
  sigfillset(&sigtstp.sa_mask);
  sigtstp.sa_flags = 0;
  sigaction(SIGTSTP, &sigtstp, NULL);



  while(1) // Any variable initialized in the loop needs to be reinitialized for each input loop, 
  {        // which is why it's done in the loop
    // Creates struct for SIGINT. Sets the handler so that SIGINT will be ignored
    // Will change if foreground command is entered. in the while loop so that it returns
    // to ignore mode if changed by a foreground command
    struct sigaction sigint = {0};
    sigint.sa_handler = SIG_IGN; 
    sigfillset(&sigint.sa_mask);
    sigint.sa_flags = 0;
    sigaction(SIGINT, &sigint, NULL);
    
    // Initializing variable that states whether current command is in background
    int command_background = 0;
    // Resets array for new loop on input. No effect if this is the first loop
    reset_array(arguments);
    // Strings to hold the names of the input output files for redirection
    char input_name[MAX_CHARACTERS] = {0};
    char output_name[MAX_CHARACTERS] = {0};
    // Get and process user input from command line
    input(arguments, process_id, &command_background, input_name, output_name); 
    // Resets the no_signal variable so that the next call of input will proceed as normal
    if (! no_signal){
      no_signal = 1;
      continue;
    }
    if (arguments[0] == NULL){ // Handling blank input
      continue;
    } else if (arguments[0][0] == '#'){  // Handling commments
      continue;
    } else if (strcmp(arguments[0], "exit") == 0){ // Handling exit command
      exit_kill(backgroundPIDs); // Calls function to kill any processes running in background before exiting
      break; // Ends program
    } else if (strcmp(arguments[0], "cd") == 0){ // cd built in
      // If no path is given, we change to the home directory
      if (arguments[1] == NULL){ 
        chdir(getenv("HOME"));
      } else {
        // If a path is given we change to that path
        int chdir_result = chdir(arguments[1]);
        if (chdir_result == -1){
          // Print a text error if the path is invalid
          printf("Directory not found: %s\n", arguments[1]);
          fflush(stdout);
          continue;
        }
      }
    } else if (strcmp(arguments[0], "status") == 0){ // status built in
      // Prints the exit status of the last foreground process ran
      // Different message for a normal exit vs. signal termination
      if (exit_status > 1){
        printf("terminated by signal %d\n", exit_status);
      } else{
        printf("exit value %d\n", exit_status);
      }
      fflush(stdout);
      continue;
    } else{ // Handling non-built in commands
      command_execution(arguments, &exit_status, command_background, backgroundPIDs, 
                        &background_index, sigint, input_name, output_name);
      continue;
    } 
  }
}

