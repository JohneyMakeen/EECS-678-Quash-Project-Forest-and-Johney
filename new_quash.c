#include "quash.h"

static void sfree(char **p) { //safe free for heap stringsl also NUlls the pointer to avoid double free errors 
    if (p && *p) { free(*p); *p = NULL;}
}

static inline void clear_buf(char *s) {//clear a writeable string buffer without changing its current allocation
    if (s) s[0] = '\0';
} 
// frees a Null-terminated array of C strings created with strdup () calling malloc() to allocate new memory for each string.
// maloc() and realloc() is also directly used to allocate the array that holdes the C strings
static void free_argv(char **argv) { 
    if (!argv) return; // nothing to free if the array pointer is NULL
    for (int i = 0; argv[i] != NULL; i++) free(argv[i]); //Free each string
    free(argv); // then free the array itself 
}

/* This will be our background job representation by the shell:
id : shell-visible job id (1 ,2, 3..etc) that we assign
pid: OS process id 
cmdline: human-readable command line we ran 
active: determines weather if this slot is currently in use or not
*/
typedef struct {
    int   id;
    pid_t pid;
    char *cmdline;
    bool active;
} Job;
#define MAX_JOBS 128 //maximum concurrent background we track
static Job jobs[MAX_JOBS]; //fixed-job size job table
static int next_jobid = 1; // Next job id to assign when we add a job 

static int jobs_add(pid_t pid, const char *cmdline) { // from here this is how we add our background jobs to the fixed size array table. 
                                                        
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) { // find the first free row
            jobs[i].active = true; //mark that we are currently using it
            jobs[i].pid = pid; //store the child's process ID
            jobs[i].id = next_jobid++; // assign a shell-visible ID 
            jobs[i].cmdline = strdup(cmdline ? cmdline : ""); // allocate new memmory, and store a copy of the command line string )or an empty string if cmdline was NULL) in jobs[i].cmdline
            return jobs[i].id; //return the job id to print and use command &
        } /* for deeper understanding, eadh job needs its own copy of the command line test so that later
        when quash lists or  completes all of its jobs. it can be able to print line 62 without any issues
        */
    }
    return -1; //if all slots are full, then return -1, to signal that something went wrong
}

static Job* jobs_find_by_pid(pid_t pid) { // goal: locate the job that corresponds to the specific child process when jobs is called, 
    for (int i = 0; i < MAX_JOBS; i++)  //searching throughout the table
        if (jobs[i].active && jobs[i].pid == pid)  //return the live row matching at the specific pid
        return &jobs[i];  
    return NULL; //if yhere is nothing found, unkown pid
}

static void jobs_mark_done(pid_t pid) {// when the background child exists, we print a completed statement, and free its row to avoid memory leakage
    Job *j = jobs_find_by_pid(pid); // look up which row corresponds to the finished child
    if (j) {
        printf("Completed: [%d] %d %s\n", j->id, j->pid, j->cmdline); //if found, print the completed statement
        fflush(stdout); //making sure the message isnt stuck in a buffer
        j->active = false; //frees the slot for future jobs
        free(j->cmdline); // Release the stored command string
        j->cmdline = NULL; //pointer to avoid any sort of accidental resue. once one of the jobs is completted, move on to the next
    }
}

static void jobs_list(void) {//prints the list of the currently running background jobs in shell cylce
    for (int i = 0; i <MAX_JOBS; i++) { //walking through the table amd looking only for jobs that are currently running
        if (jobs[i].active) {
            printf("[%d] %d %s &\n", jobs[i].id, jobs[i].pid, jobs[i].cmdline); //print the jobs that ae  running and their details
        }
    }
    fflush(stdout); //prints immediately so it is not stuck in a buffer
}

static void sigchld_handler(int sig) {// as soon as a child finshes/stops, we check which child has finished, mark it as done and clear it from memory. avoiding zombie processes
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { //checks if any child processes have finished, WNOHANG prevents the code from being blocked, and not immedieately ending if no child is found
        jobs_mark_done(pid);
    }
}

static void join_argv(char *const *argv, char *out, size_t outcap) {// turn an argv array intp a human readable cmdline string. 
    //we then bound all appends to avoid overruns while trying keep as much as we can. 
    out[0] = '\0'; //start with a null buffer
    for (int i = 0; argv[i]; i++) { //for each token in the vector
        if (i && strlen(out) + 1 < outcap) //if it is not the first token, add a seperating space
            strncat(out, " ", outcap - 1 - strlen(out)); //append a space
        strncat(out, argv[i], outcap - 1 - strlen(out)); //append the token
    }
}

static bool is_builtin(const char *s) { //since builtins are handed by the shell itself, and not by the user executing it: This will help make sure that the user is not launching an external program
    return s && 
    (!strcmp(s, "echo") || !strcmp(s, "pwd") || !strcmp(s, "cd") || 
    !strcmp(s, "export") || !strcmp(s, "jobs") || !strcmp(s, "kill") ||
    !strcmp(s, "exit") || !strcmp(s, "quit"));
}

int main(void){
    /*  - input line will hold whatever text the user inputes. 
    - getline() will allocate/resize it.
    - buffer size will tell us how much memory is available in the input line
    */
    char *input_line = NULL;
    char **argv = NULL; // array to store our space deliminated argument
    size_t buffer_size = 0;
    char *result = NULL; // maximum byte size of result is 256

    printf("Welcome to Johney and Forest Quash!\n");

    struct sigaction sa = {0}; //prepare a sigaction struct
    sa.sa_handler = sigchld_handler; // routes sighild to our sigchildhandler function
    sigemptyset(&sa.sa_mask); //makes sure we dont block other signals while being inside the handler
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; //this helps with restarting syscalls and not terminating the process. ignore child stop/continue events
    sigaction(SIGCHLD, &sa, NULL); //tells OS to use this signal handler 


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

        result = run_args(argv);  // runs the arguments

        printf("%s\n", result); // prints out the end result of the run_args function
        free(result);
        result = NULL;

        // now to free up memory57
        for (int i = 0; argv[i] != NULL; i++) {
            free(argv[i]);  // free up each arg
        }
        free(argv); // now that it's empty, we cn free it
        // free_argv(argv);

    }

    // at this point, the quash program is ending, and we're just freeing memory
    free(input_line);
    free(result);
    // now to free up args
    printf("Adios Amigo!\n");
    return 0;
}
/* Helps with comamnds like export PATH=$HOME. Main idea is that it expands a single token for those like $HOME.
If it starts with $, copy the env value and keep any of the after things (i.e. $HOME/bin).
Otherwise, copy the token unchanged. 
*/
static void expand_env_token(const char *tok, char *out, size_t outcap) { 
    if (!tok || !out || outcap == 0) return; //if it is an invalid output buffer
    out[0] = '\0'; //starts with empty output

    if (tok[0] != '$') { 
        strncat(out, tok, outcap - 1); //copy 
        return;
    }

    const char *p = tok + 1; //begins scanning the variable name after '$'
    while (*p && (isalnum((unsigned char)*p) || *p == '_')) {// POSIX variable chars. isalnum will help with checking if the characther is either a letter or decimal digit
        p++; //continue with the name while it is still valid
    }

    size_t len_of_name = (size_t)(p- (tok + 1));// calculate how many charcathers we saw throuhout the length of the name

    if (len_of_name == 0) { //edge case
        strncat(out, tok, outcap - 1);
        return;
    }

    char name[256]; //temp buffer for the variable name 
    if (len_of_name >= sizeof(name)) { //if the name is too long
        strncat(out, tok, outcap - 1); //go back to the previous token then
        return; //avoid overflows
    }

    memcpy(name, tok + 1, len_of_name); //copy just the name portion info stack buffer 
    name[len_of_name] = '\0'; //making sure it is a proper C string

    const char *val = getenv(name); //reading the enviorment variable 
    if (!val) val = ""; // unset means empty

    strncat(out, val, outcap - 1 - strlen(out)); //First, place the enviorment value
    strncat(out, p, outcap - 1 - strlen(out)); //then append any suffix after the name 
}

/* reading tokens from left to right. supporting background programs with '&
piping string handoff, builtins (pwd,cd, exprt, jobs, kill, echo). returns the final stages output text
*/

char *run_args(char **argv){  // Loop that can take it's original output as a later input

    int arg_num = 0;  // index to what argument we're on
    char *output = malloc(256);  // what is being outputed by the current function, can also be used to redirect output
    output[0] = '\0';
    char *input = malloc(256); // just a string to store the redirected output

    int argc = array_length(argv); //count tokens
    bool run_in_background = false; //track if user asked for async exec
    if (argc > 0 && strcmp(argv[argc-1], "&")== 0) { //if last token is &, its a background request
        run_in_background = true; //flip to async behaviour. 
        free(argv[argc-1]); //& was recently heap duplicated, to avoid copies we need to free it
        argv[argc-1] = NULL; //helps with later code
        argc--; 
    }

    if (run_in_background && !is_builtin(argv[0])) { //only external commands get backgrounded here
        char *argv2[128]; //local scratch argv unti; the first control symbol
        int k = 0; //keeps count 
        for (int i = 0; argv[i]; i++) {
            if (!strcmp(argv[i], "|") || !strcmp(argv[i], "<") || !strcmp(argv[i], ">") || 
                !strcmp(argv[i], ">>") || !strcmp(argv[i], "#")) break;

            argv2[k++] = argv[i];
    }

    argv2[k] = NULL;
    
    char cmdline[512];
    join_argv(argv2, cmdline, sizeof(cmdline));

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
    } else if (pid == 0) {
        execvp(argv2[0], argv2);
        perror("execvp");
        _exit(127);
    } else {
        int jid = jobs_add(pid, cmdline);
        printf("Background job started: [%d] %d %s &\n", jid, pid, cmdline);
        if (jid == -1) {
            fprintf(stderr, "Error: too many background jobs\n");
        }
        fflush(stdout);
    }
    free(input);
    output[0] = '\0';
    return output;
}

    while (argv[arg_num] != NULL && (strcmp(argv[arg_num],"#" ) != 0)){ // while there's still an argument needing to be executed
        // printf("curr string: %s\n", argv[arg_num]); // helper function when need be
        if (strcmp(argv[arg_num], "|") == 0){  // piping function skeleton
            // printf("I'm in the | function! cur arg: %s, next arg: %s, cur arg num:%d\n", argv[arg_num], argv[arg_num+1], arg_num);
            sfree(&input); //free the old input buffer first
            input = strdup(output); // if the output from the previous function has been defined, we'll use it
            printf("input: %s\n", input);
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
            input[0] = '\0'; // if we have a new command that takes no input, than the previous input is null
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
            arg_num += 1; // consume the cd
            if (input[0] == '\0'){ // if we don't have outside input
                if(strcmp(argv[arg_num],"<") == 0){ // if it's this character
                    char *tmp = read_file(argv[arg_num+1]); // then open the file in the next arg_num val and cd into it
                    cd(tmp);
                    free(tmp);
                    arg_num++;
                    clear_buf(output); //keep using the existing heap buffer
                    if (argv[arg_num] != NULL) arg_num += 2;
                    else arg_num += 1;
                    continue;
                }else{

                    char *msg = cd(argv[arg_num]);
                    output[0] = '\0';
                    // if (msg && msg[0] != '\0') {
                    //     strncat(output, msg, 255 - strlen(output));
                    // }
                    clear_buf(output); //keep using the existing heap buffer
                    if (argv[arg_num] != NULL) arg_num += 2;
                    else arg_num += 1;
                    continue;
                }
        }else{  // if we're using outside input, aka from a pipe
            char *msg = cd(input); // cd into that instead
            output[0] = '\0';
            // if (msg && msg[0] != '\0') {
            //     printf("%s",msg);
            //     strncat(output, msg, 255 - strlen(output));
            // }
           clear_buf(output); //keep using the existing heap buffer
            input[0] = '\0';

            continue;
        }
        }

        if (strcmp(argv[arg_num], "export") == 0) {
            arg_num++; // consume the export
            if (input[0] == '\0'){ // if we don't have outside input
                if(strcmp(argv[arg_num],"<") == 0){
                    char *tmp = export(read_file(argv[arg_num + 1]));
                    char *msg = export(tmp);
                    free(tmp);
                    output[0] = '\0';
                    if (msg && msg[0] != '\0') {
                        strncat(output, msg, 255 - strlen(output));
                    }
                    if (argv[arg_num + 2] != NULL) arg_num += 2;
                }else{

                char *msg = export(argv[arg_num]);
                output[0] = '\0';
                if (msg && msg[0] != '\0') {
                    strncat(output, msg, 255 - strlen(output));
                }
                arg_num += 1;
                continue;
            }
            }else {  // if we have pipe input
                char *msg = export(input);
                output[0] = '\0';
                if (msg && msg[0] != '\0') {
                    strncat(output, msg, 255 - strlen(output));
                }
                input[0] = '\0';

            }
        }

        if (strcmp(argv[arg_num], "jobs") == 0) {
            jobs_list();
            output[0] = '\0';
            arg_num++;
            continue;
        }

        if (strcmp(argv[arg_num], "kill") == 0) {
            char *sign_s = argv[arg_num+1];
            char *pid_s = argv[arg_num+2];
            if (!sign_s || !pid_s) {
                fprintf(stderr, "kill: usage: kill SIGNUM PID \n");
                arg_num += 1;
            } else {
                int sign = atoi(sign_s);
                pid_t pid = (pid_t) strtol(pid_s, NULL, 10);
                if (kill(pid, sign) != 0) perror("kill");
                arg_num += 3;
            }
            output[0] = '\0';
            continue;
        }

        if (strcmp(argv[arg_num], "echo") == 0){
            arg_num++; // make is so we're looking at the arguments
            output[0] = '\0';
            char piece[512]; // if we're echoing, we're starting with a new output
            if (input[0] == '\0'){ // if we're not using outside input
                if(strcmp(argv[arg_num],"<") == 0){  // if we're needing to take a file as input
                    arg_num++; // no longer looking at "<"
                    expand_env_token(read_file(argv[arg_num]), piece, sizeof(piece));
                    if (output[0] != '\0'){
                        strncat(output, " ", 255 - strlen(output));
                    }
                    strncat(output, piece, 255 - strlen(output));
                    arg_num++; // just to make up for the lack of loop
                    // printf("from echo: %s\n", output);
                    continue;

                }else{
                for (;argv[arg_num] != NULL ; arg_num++){ // while there isn't a stop
                    if (strcmp(argv[arg_num],"|" ) == 0 || 
                        strcmp(argv[arg_num],">" ) == 0 || 
                        strcmp(argv[arg_num],"#") == 0 || 
                        strcmp(argv[arg_num],">>" ) == 0){
                        break; // if there's any strings that break the regular flow of echo
                        }
                    expand_env_token(argv[arg_num], piece, sizeof(piece));
                    if (output[0] != '\0'){
                        strncat(output, " ", 255 - strlen(output));
                    }
                    strncat(output, piece, 255 - strlen(output));
                }
                // printf("from echo: %s\n", output);
                continue;
            }
            }
            else{  // if we have outside input
                    expand_env_token(input, piece, sizeof(piece));
                    if (output[0] != '\0'){
                        strncat(output, " ", 255 - strlen(output));
                    }
                    strncat(output, piece, 255 - strlen(output));
                    input[0] = '\0';
                    continue;
                }
        }

        
        else{  // if we haven't found the argument yet, we're going to assume it's a linux command
            // printf("invalid argument: %s\n", argv[arg_num]);
            // it might be a linux system function, so we need to prepare it
            output[0] = '\0'; // if we're changing it, we're starting with a new output
            if (input[0]=='\0'){
                for (;argv[arg_num] != NULL ; arg_num++){ // while there isn't a stopping symbol
                    if (strcmp(argv[arg_num],"|" ) == 0 || 
                        strcmp(argv[arg_num],">" ) == 0 || 
                        strcmp(argv[arg_num],"#") == 0 || 
                        strcmp(argv[arg_num],">>" ) == 0){
                        arg_num--; // just to ensure we don't go past this later
                        break; // if there's any strings that break the regular flow of the function
                    }
                    if (output[0] == '\0'){  // if output is empty
                        strcpy(output, argv[arg_num]);  // just set it equal to the argument
                    }else{
                        strncat(output, " ", strlen(" "));
                        strncat(output, argv[arg_num], strlen(argv[arg_num])); // concatinate the two together
                    }
                }
                arg_num++;
                { //help with memory leakage
                    char *cmd = strdup(output); //safe copy of the command string
                    sfree(&output); //free old 256-byte buffer
                    output = run_command(cmd); //run and get a new heap string
                    free(cmd); //free the temp cmd copy
                    if(!output) output = strdup(""); //prevent a crash if a command does fail
                }
            }else{  // if we have outside input
                if (output[0] == '\0'){
                    strcpy(output, argv[arg_num]); // make it so the output has the linux command
                }
                strncat(output, " ", 1);
                strncat(output, input, strlen(input));
                {
                    char *cmd = strdup(output);
                    sfree(&output);
                    output = run_command(cmd);
                    free(cmd);
                    if (!output) output = strdup("");
                    input[0] = '\0';
                    arg_num++;
                }
                input[0] = '\0'; // now we need to clear the input
                arg_num++;
                }
            
            // now to call have our output put into the linux system calls
        }

            // snprintf(output, sizeof(output), "Invalid argument \"%s\"",argv[arg_num]);  // we make the output this string
    }

    // now we're done running arguments
    free(input);
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


char* run_command(const char* cmd) {  // function that deals with running a linux command and returning it's output
    FILE *fp;
    char buffer[1024];
    size_t output_size = 1; // starts with 1 just so we can get the null terminator
    char *output = malloc(output_size);  
    if (!output) return NULL;
    output[0] = '\0';

    fp = popen(cmd, "r");  // opens a pipe to a process and reads the output
    if (!fp) {  // if it didin't work
        free(output);
        return NULL;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {  // if there's still stuff in the pipe
        size_t len = strlen(buffer);
        char *temp = realloc(output, output_size + len);  // make sure output can store it
        if (!temp) {  // if temp doesn't exist
            free(output);
            pclose(fp);
            return NULL;
        }
        output = temp; // set our output = to the value we now know is valid
        strcat(output, buffer);  // concatinate the buffer to the output 
        output_size += len;
    }

    pclose(fp);
    return output;
}

char* read_file(const char* filename) { // takes in a file name, exports out a string of the file
    FILE *file = fopen(filename, "r");
    if (!file) { // if the file doesn't open
        perror("Failed to open file");
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    char *buffer = malloc(255); // how many bytes we're storing
    if (!buffer) {
        perror("malloc failed");
        fclose(file);
        char *empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    // max of 256 bytes
    size_t read_size = fread(buffer, 1, 256, file);
    buffer[read_size] = '\0';  // null terminated

    fclose(file);
    return buffer;
}


char *write_file(const char* filename, const char* content) {
    FILE *file = fopen(filename, "w");  // begin writting to file
    if (!file) {
        perror("Failed to open file");
        return ""; 
    }

    // how many bytes we need to write up to
    size_t len = strnlen(content, 256);
    size_t written = fwrite(content, 1, len, file);
    if (written != len) {
        perror("fwrite failed");
        fclose(file);
        return "";
    }

    fclose(file);  // close the file
    return "";  // if everything goes well
}
  



