
#define _GNU_SOURCE // enables GNU extension for getline()
#include <stdio.h> // I/O and printf, getline, perror
#include <stdlib.h> //  exit and free
#include <string.h> // strcmp, strcspn  string helpers
#include <unistd.h> // fork, execlp
#include <sys/types.h> // pid_t
#include <sys/wait.h> //waits
#include <stdbool.h> // booleans

char **split_into_args(const char *input);
char *run_args(char **argv);
char *pwd(void);
int array_length(char **arr);
int main(void){
    /*  - input line will hold whatever text the user inputes. 
    - getline() will allocate/resize it.
    - buffer size will tell us how much memory is available in the input line
    */
    char *input_line = NULL;
    char **argv = NULL; // array to store our space deliminated argument
    size_t buffer_size = 0;

    printf("Welcome to Johney and Forest Quash!\n");

    while(1){ // infinite loop until it recieves the exit command
        printf("[QUASH]$ ");
        fflush(stdout); //prints the promp, and fflush will help the prompt appear immediately

        /* - getline returns the number of charachters read from the input. 
        - which will be held by n. 
        - when getline() succeeds, it will return a positive number (the number of bytes read).
        - if it fails, or reaches the end of thef file, it will return a -1
        */
        if (getline(&input_line, &buffer_size, stdin)< 0) break; // if getline failed and returned -1


        /* strcspn = string complement span: a function that will 
        find where the new line is (getline() ends with \n, which may later cause issues)
        and replaces it with a string terminator (so \n is gone), and marks the end of a string.
        */

        input_line[strcspn(input_line, "\n")] = '\0';

        if (input_line[0] == '\0'){
            continue; //ignoring blank input, that way no error occurs and program does not end
        }
        
        argv = split_into_args(input_line); // deliminates the input line by a space into a array of chars, terminated by NULL
        // no longer need input line, since argv has done all the heavy lifting
        
        // now, before we start running anything, we need to see if there's an & at the end to make sure it runs in the background
        // argv_len = array_length(argv); // function that just returns length of array
        // if (argv_len > 2){  // just to make sure it doesn't break, we know we'll have at least 1 argument, 1 NULL
        //     if (argv[argv_len - 2] == '&'){ // if we have to run the functions in the background
        //         argv[argv_len] = NULL; // remove the & sign to not interfer when this is passed down
        //         free(argv[argv_len -1]);

        //         /* TODO:
        //         Make a function that takes in the argvs, and runs them as a background process to the run_args function
        //         */

        //         continue; // if we've started the execution, we can go back to the beginning while it works in the background
        //     }
        // }
        if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0){  // if the command is quit or exit. Need to run this before the rest
            break;
        }

        char *result = run_args(argv);
        printf("%s\n", result);
        free(result);


        



    

    }
    // at this point, the quash program is ending, and we're just freeing memory
    free(input_line);
    // now to free up args
    for (int i = 0; argv[i] != NULL; i++) {
        free(argv[i]);  // free up each arg
    }

    printf("Adios Amigo!\n");
    return 0;
}
char *join_args(char **argv, int start) {
    int total_len = 0;
    int count = 0;

    for (int i = start; argv[i] != NULL; i++) {
        total_len += strlen(argv[i]) + 1;
        count++;
    }

    if (count == 0) {
        return strdup(""); //echo with no arguments 
    }
    char *result = malloc(total_len);
    if (!result) return NULL;
    result[0] = '\0';

    for (int i = start; argv[i]; i++) {
        strcat(result, argv[i]);
        if (argv[i + 1] != NULL)
            strcat(result," ");
    } 
    return result;
}

char *run_args(char **argv){  // recursive function that calls all the command

    if (strcmp(argv[0], "pwd") == 0){
        return pwd();
    }else if (strcmp(argv[0], "echo") == 0) {
        char *joined = join_args(argv, 1);
        return joined;
    } else {
        return strdup("Invalid argument");
    }
}

/* pwd command: 
- getcwd (current working directory) will ask the OS for the current directory path
and allocates memory for it
*/
char *pwd(){  // this'll be the general structure. Have ea
    char* cwd = getcwd(NULL, 0); 
    if (cwd) { //if its sucessful, return it
        return cwd;
    } else {
        return "cwd did not work, bug fix it";
    }

}


char **split_into_args(const char *input){  // turns a input string into a space delimanted char array
    char* temp = strdup(input);  // duplicate string since strtok modifies it
    int capacity = 10;  // initial capacity for array of strings
    char** result = malloc(capacity * sizeof(char*));
    if (!result) {
        free(temp);
        return NULL;
    }

    int i = 0;
    char* token = strtok(temp, " ");
    while (token != NULL) {
        if (i >= capacity) {
            capacity *= 2;
            char** new_result = realloc(result, capacity * sizeof(char*));
            if (!new_result) {
                // cleanup on realloc failure
                for (int j = 0; j < i; j++){
                    free(result[j]);
                }
                free(result);
                free(temp);
                return NULL;
                }
            result = new_result;
            }
        result[i++] = strdup(token);
        token = strtok(NULL, " ");
    }
    result[i] = NULL; // Null terminated array
    free(temp);
    return result;
}

int array_length(char **arr) {
    int count = 0;
    while (arr[count] != NULL) {
        count++;
    }
    return count;
}

bool is_valid_function(const char *func){  // helper function to determine if a str is a valid quash function
    const char *valid_funcs[] = {
        "cd",
        "pwd",
        "echo",
        "set",
        "export",
        "jobs",
        "kill",
        NULL // NULL terminated array
    };
    if (strncmp(func, "./", 2) == 0) { // if it's a ./ function, likely with a executable to run
        return true;
    } 

    for (int i = 0; valid_funcs[i] != NULL; i++) {  // for every one of our valid functions
        if (strcmp(func, valid_funcs[i]) == 0) { // if it's a regular quash function
            return true;  // then we return true
        }
    }

    return false; // But if nothings returned true yet, then we don't have any valid code
}

