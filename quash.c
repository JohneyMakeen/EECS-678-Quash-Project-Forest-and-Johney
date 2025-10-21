/* Install a SIGCHLD handler to help with bakcground command lines. 
when a background command line finishes, quash should print "[JOBID] PID COMMAND " 
In other words, SIGCHLD will be a way to communicate to the parent process
that the child has either terminated or changed state.
Without SIGCHLD, we would not be able to print when background job finishes, 
zombie processes would be created, and the shell would not be able to continue
without blocking. */
#define _GNU_SOURCE // enables GNU extension for getline()

int main(void) {
    struct sigaction sa = {0};

}

