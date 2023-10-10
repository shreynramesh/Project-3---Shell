#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "wsh.h"

struct Job* job_list[256] = {NULL};
struct Process* proc_list[256] = {NULL};

struct Job* initJob(int fg) {
    for(int i = 0; i < 256; i++) {
        if(job_list[i] == NULL) { // Lowest Avaliable job id
            job_list[i] = malloc(sizeof(struct Job));
            job_list[i]->jid = i;
            job_list[i]->fg = fg;
            job_list[i]->status = RUNNING;

            for(int j = 0; j < 256; j++) {
                if(proc_list[j] != NULL) {
                    proc_list[j]->pjid = i;
                    job_list[i]->processes[j] = *proc_list[j];  
                }
            }

            return job_list[i];
        }
    }

    return NULL;
}

void initProcess(struct Process* process, pid_t pid, pid_t pgid, int argc, char **argv, bool finalized) {
    if(pid != -1) {
        process->pid = pid;
    }
    
    if(pgid != -1) {
        process->pgid = pgid;
    }
    
    if(argc != -1) {
        process->argc = argc;
    }
    
    if(argv != NULL) {
        for (int i = 0; i < argc; i++) {
            process->argv[i] = strdup(argv[i]);
        }
    }
    
    if(finalized) {
        for(int i = 0; i < 256; i++) { // Adding process to proc_list
            
            if(proc_list[i] == NULL) {
                proc_list[i] = process;
                break;
            }
        }
    }
    
}

void moveJobToFg(struct Job *job) {
    // Mark previous fg job as bg
    for(int i = 0; i < 256; i++) {
       if(job_list[i] != NULL && job_list[i]->fg == 1) {
            job_list[i]->fg = 0;
        }
    }

    // Mark target_job as fg and move to fg
    job->fg = 1;

    signal (SIGTTIN, SIG_DFL);
    signal (SIGTTOU, SIG_DFL);
    signal (SIGINT, SIG_DFL);
    signal (SIGTSTP, SIG_DFL);
    tcsetpgrp(STDIN_FILENO, job->pgid);
}

void parseInput(struct Process* process, char* ibuf) {
    char* arg;
    int argc = 0; // arg index
    char *argv[256];

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
    
                // process->argv[i] = malloc(3 * sizeof(char)); // "-flag\0"
                // if (process->argv[i] == NULL) {
                //     perror("Memory allocation error");
                //     exit(-1);
                // }
                char str[3];
                sprintf(str, "-%c", flag);
                argv[argc] = strdup(str);
                argc++; 
            }
        } else {
            // process->argv[i] = malloc((strlen(arg) + 1) * sizeof(char));
            // if (process->argv[i] == NULL) {
            //     perror("Memory allocation error");
            //     exit(-1);
            // }
            char str[strlen(arg) + 1];
            sprintf(str, "%s", arg);
            argv[argc] = strdup(str);
            argc++; 
        }
    }

    initProcess(process, -1, -1, argc, argv, false); // Will init pid and pgid after fork()
}

// void fg(struct Job** joblist, int jid) {
//     struct Job* job;
//     if(jid == -1) { // Finding largest jid if no job is specified
//         for(int i = 255; i >= 0; i++) {
//             if(joblist[i] != NULL) {
//                 job = joblist[i];
//             }
//         }
//     } else {
//         job = joblist[jid];
//     }

//     if(job != NULL) {
//         struct Job* fgJob;
//         if(job->fg != 1) {
//             for(int i = 0; i < 256; i++) { // Finding current fg job
//                 if(joblist[i] != NULL && joblist[i]->fg == 1) {
//                     fgJob = joblist[i];
//                     printf("Current FG Job: %d\n", fgJob->jid)
//                     break;
//                 }
//             }
//         }
//     } else {
//         perror("No jobs to move to fg");
//         exit(-1);
//     }
// }

struct Job* findJob(int jid) {
    for (int i = 0; i < 256; i++) {
        if(job_list[i] != NULL && job_list[i]->jid == jid) {
            return job_list[i];
        }
    }

    return NULL;
}

struct Job* findLargestJob() {
    for (int i = 256; i >= 0; i--) {
        if(job_list[i] != NULL) {
            return job_list[i];
        }
    }

    return NULL;
}

bool handleBuiltInCommand(struct Process* process, struct Job** joblist) {
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

    if(strcmp(process->argv[0], "fg") == 0) {
        struct Job* job;
        if(process->argc > 2) {
            perror("Invalid number of arguments");
            exit(-1);
        } else if (process->argc == 2) {
            job = findJob(process->pjid);
            if(job == NULL) {
                perror("Job id not found");
                exit(-1);
            }
        } else {
            job = findLargestJob();
            if(job == NULL) {
                perror("Job list empty");
                exit(-1);
            }
        }

        moveJobToFg(job);
    }

    return false;
}

void removeJob(struct Job** joblist, int jid) {
    for(int i = 0; i < 256; i++) {
        if(joblist[i] != NULL) {
            if(joblist[i]->jid == jid) {
                free(joblist[i]);
                joblist[i] = NULL;
                break;
            }
        }     
    }
}

void clearProcessList() {
    for (int i = 0; i < 256; i++) {
        proc_list[i] = NULL;
    }
}

void printProcList() {
    for(int i = 0; i < 256; i++) {
        if(proc_list[i] != NULL) {
            printf("\tProcess: %s, PID: %d, PGID: %d ", proc_list[i]->argv[0], proc_list[i]->pid, proc_list[i]->pgid);
            for(int j = 0; i < proc_list[i]->argc - 1; j++) {
                if(proc_list[i]->argv[j] != NULL) {
                    printf("Args: %s ", proc_list[i]->argv[j]);
                }
            }
            printf("\n");
        }
    }
}

void printJobList() {
    for(int i = 0; i < 256; i++) {
        if(job_list[i] != NULL) {
            printf("Job ID: %i, FG: %i, Status: %d, Process Name: %s, PID: %d, PGID: %d\n", job_list[i]->jid, job_list[i]->fg, job_list[i]->status, job_list[i]->processes[0].argv[0], job_list[i]->processes[0].pid, job_list[i]->processes[0].pgid);
        }
    }
}

int main(int argc, char *argv[]) {

    // Setting up shell process
    signal (SIGINT, SIG_IGN);
    signal (SIGTSTP, SIG_IGN);
    pid_t shell_pid = getpid();

    struct Process shell_proc;
    initProcess(&shell_proc, shell_pid, shell_pid, argc, argv, true);// Everything exceept pjid which will be initialized in initJob

    // Adding shell to joblist
    struct Job* shell_job;
    if((shell_job = initJob(1)) == NULL) {
        perror("Job list full");
        exit(-1);
    }

    while (true) {
        // int status;

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
        struct Process child_proc = {0};
        parseInput(&child_proc, ibuf);

        printf("Args: \n");
        for(int i = 0; i < child_proc.argc; i++) {
            printf("%s\n", child_proc.argv[i]);
        }
        printf("Num Args: %i\n", child_proc.argc);

        clearProcessList();

    //     // Handle all built-in commands: exit, cd, jobs, fg, bg 
    //     if(handleBuiltInCommand(&process, joblist)) {
    //         continue;
    //     }
        // Forking shell in order to run user command
        pid_t child_pid = fork();

        // Child process setup - Everything except pjid which will be initialized in in initJob
        initProcess(&child_proc, child_pid, child_pid, -1, NULL, true);
        // Adding child to job_list
        struct Job* child_job;
        if((child_job = initJob(0)) == NULL) {
            perror("Job list full");
            exit(-1);
        }

        if(child_pid < 0) { // failed fork
            perror("Fork to new process failed");
            exit(-1);
        } else if(child_pid == 0)  { // child (user) process

            // Child process setup
            setpgid(child_pid, child_pid);

            // Ensuring child_job is on the fg
            printf("Child Process: %d\n", getpgid(getpid()));
            moveJobToFg(child_job);

            // Running command
            int rc = execvp(child_proc.argv[0], child_proc.argv);
            if(rc == -1) {
                perror("Unable to execute command");
                exit(-1);
            }
        } else { // parent (shell) process
            wait(NULL);
             printf("Back to Parent Process: %d\n", getpgid(getpid()));
            moveJobToFg(shell_job);
            removeJob(job_list, child_job->jid);
        }
    }

    // Freeing resources
    for(int i = 0; i < 256; i++) {
        if(job_list[i] != NULL) {
            free(job_list[i]);
        }  
    }
}