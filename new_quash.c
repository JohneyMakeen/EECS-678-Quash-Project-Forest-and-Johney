#include "quash.h"

static void free_argv(char **argv) {
    if (!argv) return;
    for (int i = 0; argv[i] != NULL; i++) free(argv[i]);
    free(argv);
}

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
        //         Make a function that takes in the argv, and runs them as a background process to the run_args function
        //         */

        //         continue; // if we've started the execution, we can go back to the beginning while it works in the background
        //     }
        // }
        if (!argv || !argv[0]) {
            free_argv(argv);
            continue;
        }

        if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0){  // if the first command is quit or exit, break and stop. Need to run this before the rest
            free_argv(argv);
            break;
        }

        printf("%s\n", run_args(argv)); // prints out the end result of the run_args function
        free_argv(argv);

    }

    // at this point, the quash program is ending, and we're just freeing memory
    free(input_line);
    // now to free up args
    /*
    for (int i = 0; argv[i] != NULL; i++) {
        free(argv[i]);  // free up each arg
    }
*/
    printf("Adios Amigo!\n");
    return 0;
}

static void expand_env_token(const char *tok, char *out, size_t outcap) {
    if (!tok || !out || outcap == 0) return;
    out[0] = '\0';

    if (tok[0] != '$') {
        strncat(out, tok, outcap - 1);
        return;
    }

    const char *p = tok + 1;
    while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
        p++;
    }

    size_t len_of_name = (size_t)(p- (tok + 1));

    if (len_of_name == 0) {
        strncat(out, tok, outcap - 1);
        return;
    }

    char name[256];
    if (len_of_name >= sizeof(name)) {
        strncat(out, tok, outcap - 1);
        return;
    }

    memcpy(name, tok + 1, len_of_name);
    name[len_of_name] = '\0';

    const char *val = getenv(name);
    if (!val) val = ""; // unset means empty

    strncat(out, val, outcap - 1 - strlen(out));
    strncat(out, p, outcap - 1 - strlen(out));
}


char *run_args(char **argv){  // recursive function that calls all the command

    int arg_num = 0;  // index to what argument we're on
    char *output = malloc(256);  // what is being outputed by the current function, can also be used to redirect output
    output[0] = '\0';
    char *input = malloc(256); // just a string to store the redirected output
    
    while (argv[arg_num] != NULL && (strcmp(argv[arg_num],"#" ) != 0)){ // while there's still an argument needing to be executed
        // printf("curr string: %s\n", argv[arg_num]); // helper function when need be
        if (strcmp(argv[arg_num], "|") == 0){  // piping function skeleton
            // printf("I'm in the | function! cur arg: %s, next arg: %s, cur arg num:%d\n", argv[arg_num], argv[arg_num+1], arg_num);
            arg_num++;
            continue;
        }
    
        if (strcmp(argv[arg_num], ">") == 0){  // > function skeleton
            arg_num++;
            continue;
        }

        if (strcmp(argv[arg_num], ">>") == 0){  // piping function skeleton
            arg_num++;
            continue;
        }

        if (strcmp(argv[arg_num], "pwd") == 0){
            char *cwd = pwd();
            output[0] = '\0';
            if (cwd) {
                strncat(output, cwd, 255);
                free(cwd);
            } else {
                strncat(output, "pwd: getcwd failed", 255);
            }
            arg_num++;
            continue;
        }
        if (strcmp(argv[arg_num], "cd") == 0) {
            char *msg = cd(argv[arg_num + 1]);
            output[0] = '\0';
            if (msg && msg[0] != '\0') {
                strncat(output, msg, 255 - strlen(output));
            }
            if (argv[arg_num + 1] != NULL) arg_num += 2;
            else arg_num += 1;
            continue;
        }

        if (strcmp(argv[arg_num], "export") == 0) {
            char *msg = export(argv[arg_num + 1]);
            output[0] = '\0';
            if (msg && msg[0] != '\0') {
                strncat(output, msg, 255 - strlen(output));
            }
            if (argv[arg_num + 1] != NULL) arg_num += 2;
            else arg_num += 1;
            continue;
        }

        if (strcmp(argv[arg_num], "echo") == 0){
            arg_num++; // make is so we're looking at the arguments
            output[0] = '\0';
            char piece[512]; // if we're echoing, we're starting with a new output
            for (;argv[arg_num] != NULL ; arg_num++){ // while there isn't a stop
                if (strcmp(argv[arg_num],"|" ) == 0 || 
                    strcmp(argv[arg_num],">" ) == 0 || 
                    strcmp(argv[arg_num],"#") == 0 || 
                    strcmp(argv[arg_num],">>" ) == 0){
                    break; // if there's any strings that break the regular flow of echo
                }
                expand_env_token(argv[arg_num], piece, sizeof(piece));
                if (output[0] != '\0'){  // if output is empty
                    strncat(output, " ", 255 - strlen(output));
                  }  // just set it equal to the argument

                strncat(output, piece, 255 - strlen(output)); // concatinate the two together
            }
            continue;
        }

        
        else{  // if we haven't found the argument yet
            // printf("invalid argument: %s\n", argv[arg_num]);
            output = "Invalid argument ";
            break;
            // snprintf(output, sizeof(output), "Invalid argument \"%s\"",argv[arg_num]);  // we make the output this string
        }
    }
    // now we're done running arguments
    return output;
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

int array_length(char **arr) {  // helper function to determine size of an array
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

char *export(char *argv){  // will only take in 1 string as a argument, aka what to change the path to

    if (argv == NULL || argv[0] == '\0'){
        return "export: missing Name or Name = Value";
    }

    const char* eq = strchr(argv, '=');

    if (!eq) {
        const char *cur = getenv(argv);
        if (!cur) {
            if (setenv(argv, "", 1) != 0) {
                return "Export: failed to set empty value";
            }
        }
        return "";
    }

    size_t len_of_name = (size_t) (eq - argv);
    if (len_of_name == 0) {
        return "invalid name";
    }

    char name[256];
    if (len_of_name >= sizeof(name)) {
        return "export: variable name too long";
    }

    memcpy(name, argv, len_of_name);
    name[len_of_name] = '\0';    

    const char *value = eq + 1;

    if (value[0] == '$' && value[1] != '\0') {
        const char *exp = getenv(value + 1); //get what $var means
        if (!exp) exp = ""; //not found
        if (setenv(name, exp, 1) != 0) {
            return "export: failed to set variable";
        }

        return "";
    }

    if (setenv(name, value, 1)!= 0) {
        return "export: failed to set variable";
    }
    return "";
}

char *cd(char *argv){  // will take in 1 string as an argument, which will just be the directory you want to change to
    const char *target = NULL;

    if (argv && argv[0] == '$' && argv[1] != '\0') {
        const char *val = getenv(argv + 1);
        if (!val || !*val) return "cd: Target not set";
        target = val;
    }

    else if (argv == NULL || argv[0] == '\0' || strcmp(argv, "~") == 0) {
        target = getenv("HOME");
        if (!target || !*target) return "cd: HOME NOT SET";
    }

    else if (strcmp(argv, "-")==0){
        target = getenv("OLDPWD");
        if (!target || !*target) return "cd: OLDPWD not set";
    }
    else {
        target = argv;
    }

    char oldpwd_buf[4096];
    if (!getcwd(oldpwd_buf, sizeof oldpwd_buf)) {
        oldpwd_buf[0] = '\0';
    }

    if (chdir(target) != 0) {
        return "cd: no such file or directory";
    }
    if (oldpwd_buf[0] != '\0') {
        setenv("OLDPWD", oldpwd_buf, 1);
    }

    char newpwd_buf[4096];
    if (getcwd(newpwd_buf, sizeof newpwd_buf)) {
        setenv("PWD", newpwd_buf, 1);
    }
    return "";
}