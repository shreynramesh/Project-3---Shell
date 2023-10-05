#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "wsh.h"

void initJobIdList(struct JobIdList* jobids) {
    for(int i = 0; i < 256; i++) {
        jobids->jobs[i] = i + 1;
    }
}

bool handleBuiltInCommand(struct Process* process) {
    if (strcmp(process->argv[0], "exit") == 0) {
        if(process->argc > 1) {
            perror("Invalid number of arguments");
            exit(-1);
        }
        exit(0);
    }

    if(strcmp(process->argv[0], "cd") == 0) {
        if(process->argc == 1 || process->argc > 2) {
            perror("Invalid number of arguments");
            exit(-1);
        }

        if(chdir(process->argv[1]) == -1) {
            perror("Failed to change directory");
            exit(-1);
        }
        return true;
    }

    return false;
}

int getNextJobId(struct JobIdList* listPtr) {
    for(int i = 0; i < 256; i++) {
        if(listPtr->jobs[i] != -1) {
            int res = listPtr->jobs[i];
            listPtr->jobs[i] = -1;
            return res;   
        }
    }

    return -1;
}

void freeJobId(struct JobIdList* listPtr, int i) {
    listPtr->jobs[i - 1] = i;
} 

void addJobToList(struct Job job, struct JobList* joblist) {

    if(joblist->fg == NULL) { // no job has been added yet
        joblist->fg = malloc(sizeof(struct Job));
        if(joblist->fg == NULL) {
            perror("Memory allocation error");
            exit(-1);
        }

        if(joblist->bg == NULL) { // no background job has been added yet
            joblist->bg = malloc(256 * sizeof(struct Job*));
            if(joblist->bg == NULL) {
                perror("Memory allocation error");
                exit(-1);
            }
        }
    } else { // Have to move fg job to bg and new job to fg
        for(int i = 0; i < 256; i++) {
            printf("%i, %p\n", i, (void*)joblist->bg[i]); 
            if(joblist->bg[i] == NULL) {
                joblist->bg[i] = malloc(sizeof(struct Job));
                if(joblist->bg[i] == NULL) {
                    perror("Memory allocation error");
                    exit(-1);
                }

                memcpy(joblist->bg[i], joblist->fg, sizeof(struct Job));
                break;
            } else {
                printf("\tjob id: %d, process: %s\n", joblist->bg[i]->id, joblist->bg[i]->processes[0].argv[0]);
            }
        }
    }

    *(joblist->fg) = job;

    // Printing joblist - DELETE
    printf("FG:\n\tjob id: %d, process: %s\n", joblist->fg->id, joblist->fg->processes[0].argv[0]);
    printf("BG:\n");
    for(int i = 0; i < 256; i++) {
        if(joblist->bg[i] != NULL) {
            printf("\tjob id: %d, process: %s\n", joblist->bg[i]->id, joblist->bg[i]->processes[0].argv[0]);
        }
    }
}

void parseInput(struct JobIdList* jobids, struct Process* process, char* ibuf) {
    char* arg;
    int i = 0; // arg index

    // Parsing each space seperated arg
    while ((arg = strsep(&ibuf, " "))) {
        // Removing new line char at the end of command
        if(arg[strlen(arg) - 1] == '\n') {  
            arg[strlen(arg) - 1] = '\0';
        }

        // Seperating flags to their own respective dashes
        if(arg[0] == '-') {
            for (int j = 1; j < strlen(arg); j++) {
                char flag = arg[j];
    
                process->argv[i] = malloc(3 * sizeof(char)); // "-flag\0"
                if (process->argv[i] == NULL) {
                    perror("Memory allocation error");
                    exit(-1);
                }

                sprintf(process->argv[i], "-%c", flag);
                i++; 
            }
        } else {
            process->argv[i] = malloc((strlen(arg) + 1) * sizeof(char));
            if (process->argv[i] == NULL) {
                perror("Memory allocation error");
                exit(-1);
            }

            sprintf(process->argv[i], "%s", arg);
            i++;
        }
    }

    process->argc = i;
    process->pjid = getNextJobId(jobids);
}

void validateCommand(struct Process* process, char *command) {
    // char shellpaths[][40] = {""} 
    char *filepath = malloc((strlen("/usr/bin/") + strlen(process->argv[0])) * sizeof(char));
    if(filepath == NULL) {
        perror("Memory allocation error");
        exit(-1);
    }

    sprintf(filepath, "/usr/bin/%s", process->argv[0]);
    if(access(filepath, X_OK) != 0) {
        filepath = realloc(filepath, (strlen("/bin/") + strlen(process->argv[0])) * sizeof(char));
        if(filepath == NULL) {
            perror("Memory allocation error");
            exit(-1);
        }
        
        sprintf(filepath, "/bin/%s", process->argv[0]);
        if(access(filepath, X_OK) != 0) {
            perror("Command does not exist");
            exit(-1);
        }
    }
}


int main(int argc, char *argv[]) {
    // Initializing JobId List
    struct JobIdList* jobids = malloc(sizeof(struct JobIdList));
    initJobIdList(jobids);

    // Initializing JobList
    struct JobList joblist = {NULL};

    // Creating process for the shell
    struct Process shellproc = {getpid(), getNextJobId(jobids), argc, {0}};
    for (int i = 0; i < argc; i++) {
        shellproc.argv[i] = strdup(argv[i]);
    }
    printf("Shell Process: %d %d %d %s\n", shellproc.pid, shellproc.pjid, shellproc.argc, shellproc.argv[0]);

    // Initializing a new job for the shell that is placed on the fg to begin
    struct Job shelljob = {shellproc.pjid, {shellproc, {0}}};
    printf("Shell Job: %d %d\n", shelljob.id, shelljob.processes[0].pid);
    addJobToList(shelljob, &joblist);

    while (true) {
        // Prompting for user input
        char *ibuf = NULL;
        size_t len = 0;
        ssize_t read;
        printf("wsh> ");
        if((read = getline(&ibuf, &len, stdin)) == -1) { // Checking for EOF char
            if(feof(stdin)) { // Exiting if CTRL-D is pressed
                exit(0);
            }
            exit(-1);
        }

        // Parsing input for command and arguments
        struct Process process = {0};
        parseInput(jobids, &process, ibuf);

        printf("Args: \n");
        for(int i = 0; i < process.argc; i++) {
            printf("%s\n", process.argv[i]);
        }
        printf("Num Args: %i\n", process.argc);

        // Handle all built-in commands: exit, cd, jobs, fg, bg 
        if(handleBuiltInCommand(&process)) {
            continue;
        }

        // Forking shell in order to run user command
        pid_t rc = fork();
        if(rc < 0) { // failed fork
            perror("Fork to new process failed");
            exit(-1);
        } else if(rc == 0)  { // child (user) process
            printf("Child Process:\n");

            // Running command
            int rc = execvp(process.argv[0], process.argv);
            if(rc == -1) {
                perror("Unable to execute command");
                exit(-1);
            }
        } else { // parent (shell) process
            // Pre-processing to setup child job
            process.pid = getpid();
            printf("New Process: %d %d %d %s\n", process.pid, process.pjid, process.argc, process.argv[0]);

            // Initializing new job and placing in joblist
            struct Job job = {process.pjid, {process, {0}}};
            printf("New Job: %d %d\n", job.id, job.processes[0].pid);
            addJobToList(job, &joblist);

            wait(NULL); 
            printf("Back to Parent Process:\n");

            free(ibuf);  // input buffer
        }
    }

    // Freeing resources
    free(joblist.fg);
    for(int i = 0; i < 256; i++) {
        if(joblist.bg != NULL) {
            free(joblist.bg[i]);
        }  
    }
    free(joblist.bg);
}