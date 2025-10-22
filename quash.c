
#define _GNU_SOURCE // enables GNU extension for getline()
#include <stdio.h> // I/O and printf, getline, perror
#include <stdlib.h> //  exit and free
#include <string.h> // strcmp, strcspn  string helpers
#include <unistd.h> // fork, execlp
#include <sys/types.h> // pid_t
#include <sys/wait.h> //waits

int main(void) {
/*  - input line will hold whatever text the user inputes. 
    - getline() will allocate/resize it.
    - buffer size will tell us how much memory is available in the input line
 */
    char *input_line = NULL; 
    size_t buffer_size = 0;

    printf("Welcome to Johney and Forest Quash!\n");

    while (1) { //infinite loop for the shell prompt cycle
        printf("[QUASH]$ ");
        fflush(stdout); //prints the promp, and fflush will help the prompt appear immediately

/* - getline returns the number of charachters read from the input. 
- which will be held by n. 
- when getline() succeeds, it will return a positive number (the number of bytes read).
- if it fails, or reaches the end of thef file, it will return a -1
*/
        ssize_t n = getline(&input_line, &buffer_size, stdin);
        if (n < 0) break;

/* strcspn = string complement span: a function that will 
find where the new line is (getline() ends with \n, which may later cause issues)
and replaces it with a string terminator (so \n is gone), and marks the end of a string.
 */

        input_line[strcspn(input_line, "\n")] = '\0';

        if (input_line[0] == '\0')
        continue; //ignoring blank input, that way no error occurs and program does not end

/* strcmp = string compare: makes it easier where if the user types exit or quit:
it will end the program.
*/
        if (strcmp(input_line, "exit") == 0 || strcmp(input_line, "quit") == 0)
            break;

/* pwd command: 
- getcwd (current working directory) will ask the OS for the current directory path
and allocates memory for it
*/

        if (strcmp(input_line, "pwd") == 0) { //checks if the command is pwd
            char* cwd = getcwd(NULL, 0); 
            if (cwd) { //if its sucessful, print it and release the memory afterward. 
                printf("%s\n", cwd);
                free(cwd);
            } else {
                perror("pwd");
            }
            continue; //skips the fork/exec part
        }
 /* checks the first 5 characthers to be echo, since echo is 4 characthers 
 and then the space (makes it 5). 
 as soon as it matches echo_, it means the user
 typed echo_ ____.  then print the new text after echo (user message). 
 */       
        if (strncmp(input_line, "echo ", 5) == 0) { 
            printf("%s\n", input_line + 5);
            continue;
        }
        
        pid_t pid = fork(); //creating a child process: pid == 0 in child
        if (pid == 0) { //Child Process
            execlp(input_line, input_line, NULL); /*uses execlp to replace itself with the program
                                                    input line. arg0, automcatically allocates memory */
            perror("exec failed"); 
            exit(1); //if it fails, we exit the child
        } else if (pid > 0) { //parent process, blocks until the child is finished. 
            wait(NULL);
        } else { //if for some reason it fails. 
            perror("fork has unfortantely failed, womp womp");
        }
        
    }
    free(input_line); //to prevent memory leak: done with memory. otherwise remains in use.
    printf("Adios Amigo!\n");
    return 0;
}