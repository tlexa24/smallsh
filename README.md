# smallsh

A Linux-based shell in C which replicates features of well-established shells, such as bash.

## To compile and run:
* Navigate to the directory containing smallsh.c
* Run: gcc -std=c99 -o smallsh smallsh.c
* To run the program once compiled: ./smallsh

## Functionality
### 1. The Command Prompt
Uses a colon : as the prompt for user commands
* Supports lines of up to 2048 characters and commands of up to 512 arguments

### 2. Comments and Blank Lines
The shell ignores comments and blank lines, prompting the user for a new command

### 3. Variable Expansion
The shell expands any instance of '$$; into the process ID of smallsh itself

### 4. Built-in Commands
The shell supports three built-in commands:
* exit
*     Exits the shell. 
*     When ran, the shell kills any other processes that have been started before teminating the shell itself. Includes processes running in the background
* cd
*   Changes the working directory of smallsh
*   With no arguments, it changes to the directory specified in the HOME environment variable
*   If given one argument, it changes to the directory indicated in the argument. Supports both relative and absolute paths
* status
*   Prints out either the exit status or teminating signal of the last ran foreground process

### 5. Executing Non Built-in Commands
Executes non built-in commands using fork(), exec() and waitpid().
When a command is received, smallsh will fork off a child
If the command fails becasuse the command could not be found, an error is printed and exit status set to 1
The child process terminates after running a command

### 6. Input and Output Redirection
Performs redirection using dup2()
An input file redirected via stdin is opened for reading only
An output file redirected via stdout is opened for writing only. The file is truncated if it already exists and created if it does not exist
Both stdin and stdout can be redirected on the same command

### 7. Foreground vs. Background Commands
* Foreground Commands
*   Any command without an '&' at the end is executed in the foregound and smallsh waits for its completion before returning command line access and prompting the user for the next command

* Background Commands
*   Any command with an '&' at the end is ran in the background
*   smallsh does not wait for the completion of the command before prompting the user for the next command
*   smallsh will print the process ID of a background process when it begins
*   When the background command ends, a message showing its process ID and exit status is printed

### 8. Signals SIGINT and SIGSTP
* SIGINT
*   A CTRL-C command is ignored by smallsh itself
*   All currently running background processes ignore SIGINT
*   Any command currently running in the foreground terminates when it receives SIGINT
*   Normally, this signal would kill smallsh and all child processes at the same time
* SIGSTP
*   A CTRL-Z command is ignored by any foreground comamnd or background process
*   When the parent (smallsh) receives SIGSTP, it changes permission for background processes
*   When first ran, smallsh allows background processes. If it receives SIGSTP, it no longer does, and all future background processes are ran in the foreground
*   If smallsh receives another SIGSTP signal, it returns to the normal condition and again allows processes to run in the background
