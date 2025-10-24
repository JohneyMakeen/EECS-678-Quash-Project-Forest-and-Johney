#define _GNU_SOURCE // enables GNU extension for getline()
#include <stdio.h> // I/O and printf, getline, perror
#include <stdlib.h> //  exit and free
#include <string.h> // strcmp, strcspn  string helpers
#include <unistd.h> // fork, execlp
#include <sys/types.h> // pid_t
#include <sys/wait.h> //waits
#include <stdbool.h> // booleans
#include <ctype.h> // for isalnum()
#include <sys/syscall.h> // for system calls

char **split_into_args(const char *input);
char *run_args(char **argv);
char *pwd(void);
int array_length(char **arr);
int main(void);
char *cd(char *argv);
char *export(char *argv);
char *run_command(const char *cmd);
char* read_file(const char* filename);
char *write_file(const char* filename, const char* content);