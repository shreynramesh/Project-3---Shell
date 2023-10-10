#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wsh.h"

struct Job* job_list[256] = {NULL};

void trimWhitespace(char* str) {
    int start = 0;
    int end = strlen(str) - 1;

    // Find the position of the first non-whitespace character from the start
    while (isspace(str[start])) {
        start++;
    }

    // Find the position of the first non-whitespace character from the end
    while (end > start && isspace(str[end])) {
        end--;
    }

    // Shift characters to the beginning of the string
    for (int i = start; i <= end; i++) {
        str[i - start] = str[i];
    }

    // Null-terminate the trimmed string
    str[end - start + 1] = '\0';
}

bool removeAmpersand(char* str) {
    int len = strlen(str);

    // Check if the last character is an ampersand
    if (len > 0 && str[len - 1] == '&') {
        // Remove the ampersand by replacing it with a null terminator
        str[len - 1] = '\0';
        return true;  // Indicate that an ampersand was removed
    }

    return false;  // No ampersand found or string was empty
}

bool hasPipe(const char* str) {
    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == '|') {
            return true;
        }
    }
    return false;
}

void printProcess(struct Process* proc) {
    printf("\tProgram Name: %s, pid: %d, pgid: %d, Completed: %d, Stopped: %d Args: ", proc->argv[0], proc->pid, getpgid(proc->pid), proc->completed, proc->stopped);
    for (int k = 0; k < 256; k++) {
        if (proc->argv[k] != NULL) {
            printf("%s ", proc->argv[k]);
        }
    }
    printf("\n");
}

void printJobList() {
    printf("Job list:\n");
    for (int i = 0; i < 256; i++) {
        if (job_list[i] != NULL) {
            printf("Job ID: %i, is_bg: %i, pgid: %d, Command: %s\n", job_list[i]->id, job_list[i]->is_bg, job_list[i]->pgid, job_list[i]->command);
            for (int j = 0; j < 256; j++) {
                struct Process* proc = job_list[i]->processes[j];
                if (proc != NULL) {
                    printProcess(proc);
                }
            }
        }
    }
}

void removeJob(struct Job* job) {
    for(int i = 0; i < 256; i++) {
        if(job_list[i] != NULL && job_list[i]->id == job->id) {
            for(int j = 0; j < 256; j++) {
                if(job_list[i]->processes[j] != NULL) {
                    job_list[i]->processes[j] = NULL;
                }
            }

            job_list[i] = NULL;
            job = NULL;
            break;
        }
    }
}

bool isJobStopped(struct Job* job) {
    for (int i = 0; i < 256; i++) {
        if(job->processes[i] != NULL) {
            struct Process* proc = job->processes[i];
            if (!proc->completed && !proc->stopped) {
                return (false);
            }
        }
    }
    return (true);
}

bool isJobCompleted(struct Job* job) {
    for (int i = 0; i < 256; i++) {
        if(job->processes[i] != NULL) {
            struct Process* proc = job->processes[i];
            if (!proc->completed) {
                return (false);
            }
        }
    }
    return (true);
}

void updateJobStatus(struct Job* job, pid_t pid, int status) {
    for(int i = 0; i < 256; i++) {
        if(job->processes[i] != NULL) {
            struct Process* proc = job->processes[i];
            if(pid > 0) { // Process was sent a SIGTSTP
                if(proc->pid == pid) {
                    if(WIFSTOPPED(status)) {    
                        proc->stopped = true;
                    }
                }
            } else if(pid == -1) {
                proc->completed = true;
            } 
        }
    }
}

void updateCompletedJobs() {
    for(int i = 0; i < 256; i++) {
        if(job_list[i] != NULL) {
            for(int j = 0; j < 256; j++) {
                if(job_list[i]->processes[j] != NULL) {
                    struct Process* proc = job_list[i]->processes[j];
                    if(proc->pid == -1) {
                        proc->completed = true;
                    }
                }
            }
        }
    }
}

bool isBuiltInCommand(char* name) {
    char *commands[] = {"exit", "cd", "jobs", "fg", "bg"};
    for(int i = 0; i < 5; i++) {
        if(strcmp(name, commands[i]) == 0) {
            return true;
        } 
    }

    return false;
}

void cleanJobList() {
    // Checking for terminated jobs
    for(int i = 0; i < 256; i++) {
        if(job_list[i] != NULL) {
            for(int j = 0; j < 256; j++) {
                if(job_list[i]->processes[j] != NULL) {
                    struct Process* proc = job_list[i]->processes[j];
                    if(getpgid(proc->pid) == -1) {
                        proc->completed = true;
                    }
                }
            }

            // Removing job from list if it has completed
            if(isJobCompleted(job_list[i])) {
                removeJob(job_list[i]);
            }
        }
    }
}

void initProcess(struct Job* job, int argc, char** argv) {
    // Finding next avaliable process ptr in job
    for (int i = 0; i < 256; i++) {
        if (job->processes[i] == NULL) {
            job->processes[i] = malloc(sizeof(struct Process));
            job->processes[i]->argc = argc;
            
            job->processes[i]->argv = malloc(256 * sizeof(char*));
            for (int j = 0; j < argc; j++) {
                job->processes[i]->argv[j] = malloc((strlen(argv[j]) + 1) * sizeof(char));
                job->processes[i]->argv[j] = strdup(argv[j]);
            }
            job->processes[i]->completed = false;
            job->processes[i]->stopped = false;
            break;
        }
    }
}

void initJob(struct Job* job, char* command, bool is_bg, bool isBuiltInCommand, int numProcesses) {
    job->command = strdup(command);
    job->is_bg = is_bg;
    job->stdin = 0;
    job->stdout = 1;
    job->stderr = 2;
    job->pgid = -1;
    job->numProcesses = numProcesses;
    // Finding next avaliable process ptr in job_list
    if(!isBuiltInCommand) {
        for (int i = 0; i < 256; i++) {
            if (job_list[i] == NULL) {
                job->id = i + 1;
                job_list[i] = job;
                break;
            }
        }
    }
    
}

void parseInputPipes(struct Job* job, char* ibuf) {
    trimWhitespace(ibuf);
    char* command;
    char* duplicate_command = strdup(ibuf);
    bool is_bg = removeAmpersand(ibuf);
    int numProcesses = 0;

    while ((command = strsep(&ibuf, "|"))) {
        char* arg;
        int argc = 0;  // arg index
        char* argv[256];
        trimWhitespace(command);

        // Parsing each space seperated arg
        while ((arg = strsep(&command, " "))) {
            trimWhitespace(arg);

            // Removing new line char at the end of command
            if (arg[strlen(arg) - 1] == '\n') {
                arg[strlen(arg) - 1] = '\0';
            }

            // Seperating flags to their own respective dashes
            if (arg[0] == '-') {
                for (int j = 1; j < strlen(arg); j++) {
                    char flag = arg[j];
                    char str[3];
                    sprintf(str, "-%c", flag);
                    argv[argc] = strdup(str);
                    argc++;
                }
            } else {
                char str[strlen(arg) + 1];
                sprintf(str, "%s", arg);
                argv[argc] = strdup(str);
                argc++;
            }
        }

        // Initializing new process in job
        initProcess(job, argc, argv);
        numProcesses++;
    }

    // Initializing new job in joblist
    initJob(job, duplicate_command, is_bg, false, numProcesses);
}

void parseInput(struct Job* job, char* ibuf) {
    trimWhitespace(ibuf);
    char* duplicate_command = strdup(ibuf);
    bool is_bg = removeAmpersand(ibuf);
    char* arg;
    int argc = 0;  // arg index
    char* argv[256];
    trimWhitespace(ibuf);

    // Parsing each space seperated arg
    while ((arg = strsep(&ibuf, " "))) {
        // Removing new line char at the end of command
        if (arg[strlen(arg) - 1] == '\n') {
            arg[strlen(arg) - 1] = '\0';
        }

        // Seperating flags to their own respective dashes
        if (arg[0] == '-') {
            for (int j = 1; j < strlen(arg); j++) {
                char flag = arg[j];
                char str[3];
                sprintf(str, "-%c", flag);
                argv[argc] = strdup(str);
                argc++;
            }
        } else {
            char str[strlen(arg) + 1];
            sprintf(str, "%s", arg);
            argv[argc] = strdup(str);
            argc++;
        }
    }

    // Initializing new process in job
    initProcess(job, argc, argv);

    // Initializing new job in joblist
    initJob(job, duplicate_command, is_bg, isBuiltInCommand(argv[0]), 1);
}

void fg(struct Job* job, bool stopped) {
    // Placing process group on fg
    int status;
    pid_t pid = -1;
    tcsetpgrp(sh_term, job->pgid);
    job->is_bg = false;

    if(stopped) {
        if(kill(-job->pgid, SIGCONT) < 0) {
            perror("Failed to send SIGCONT");
            exit(-1);
        }
    }

    do {
        pid = waitpid(-job->pgid, &status, WUNTRACED);
        updateJobStatus(job, pid, status);
    } while (!isJobCompleted(job) && !isJobStopped(job));
    
    tcsetpgrp(sh_term, sh_pgid);
}

void bg(struct Job* job, bool stopped) {
    job->is_bg = true;

    if(stopped) {
        if(kill(-job->pgid, SIGCONT) < 0) {
            perror("Failed to send SIGCONT");
            exit(-1);
        }
    }
}

struct Job* findJob(int jid) {
    for (int i = 0; i < 256; i++) {
        if(job_list[i] != NULL && job_list[i]->id == jid) {
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

void printBackgroundJobs() {
    for(int i = 0; i < 256; i++) {
        if(job_list[i] != NULL && !isJobCompleted(job_list[i])) {
            struct Job* job = job_list[i];
            int id = job->id;

            for(int j = 0; j < 256; j++) {
                if(job->processes[j] != NULL) {
                    struct Process *proc = job->processes[j];
                    printf("%d:", id);

                    for(int k = 0; k < proc->argc; k++) { // Printing args
                        printf(" %s", proc->argv[k]);
                    }

                    if(removeAmpersand(strdup(job->command))) {
                        printf("&\n");
                    }

                    if(job->processes[i + 1] != NULL) {
                        printf("| ");
                    }
                }
            }
            printf("\n");
        }
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

    if(strcmp(process->argv[0], "fg") == 0) {
        if(process->argc != 1 && process->argc != 2) {
            perror("Invalid number of arguments");
            exit(-1);
        }

        struct Job *job = NULL;
        if(process->argc == 1) {
            job = findLargestJob();
            if(job == NULL) {
                perror("Job list is empty");
                exit(-1); 
            }
        } else {
            job = findJob(atoi(process->argv[1]));
            if(job == NULL) {
                perror("No job found with that id");
                exit(-1);
            }
        }


        fg(job, job->processes[0]->stopped);
        return true;
    }

    if(strcmp(process->argv[0], "bg") == 0) {
        if(process->argc != 1 && process->argc != 2) {
            perror("Invalid number of arguments");
            exit(-1);
        }

        struct Job *job = NULL;
        if(process->argc == 1) {
            job = findLargestJob();
            if(job == NULL) {
                perror("Job list is empty");
                exit(-1); 
            }
        } else {
            job = findJob(atoi(process->argv[1]));
            if(job == NULL) {
                perror("No job found with that id");
                exit(-1);
            }
        }


        bg(job, job->processes[0]->stopped);
        return true;
    }

    if (strcmp(process->argv[0], "jobs") == 0) {
        if(process->argc != 1) {
            perror("Invalid number of arguments");
            exit(-1);
        }
        printBackgroundJobs();
        return true;
    }

    return false;
}

void exec_process(struct Process *process, struct Job* job, pid_t pgid, int infile, int outfile, int errfile, bool is_bg, bool isInteractive) {
    if(isInteractive) {
        // Setting group id
        pid_t pid = getpid();
        if(pgid == -1) {
            pgid = pid;
        }
        setpgid(pid, pgid);
        if(!is_bg) {
            tcsetpgrp(sh_term, pgid);
        }

        // Setting stop signals back to default.
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
    }

    // Setting channels for process
    if (infile != STDIN_FILENO) {
        dup2(infile, STDIN_FILENO);
        close(infile);
    }
    if (outfile != STDOUT_FILENO) {
        dup2(outfile, STDOUT_FILENO);
        close(outfile);
    }
    if (errfile != STDERR_FILENO) {
        dup2(errfile, STDERR_FILENO);
        close(errfile);
    }

    execvp(process->argv[0], process->argv);
    perror("Coudn't execute command");
    tcsetpgrp(sh_term, sh_pgid);
    exit(-1);
}

void closeExtraPipes(int (*pipefd)[2], int infile, int outfile, int numProcesses) {
    for(int i = 0; i < numProcesses; i++) {
        for(int j = 0; j < 2; j++) {
            if(pipefd[i][j] != infile && pipefd[i][j] != outfile) {
                close(pipefd[i][j]);
            }
        }
    }
}

void exec_job(struct Job* job, bool isInteractive) {
    pid_t pid;
    int pipefd[job->numProcesses][2], infile, outfile;

    for(int i = 0; i < job->numProcesses; i++) {
        if(pipe(pipefd[i]) < 0) {
            perror("Failed to create pipe");
            exit(-1);
        }
    }

    infile = job->stdin;
    struct Process **proc_list = job->processes;
    for(int i = 0; i < job->numProcesses; i++) {
        
        if(proc_list[i] != NULL) {
            // Handling built-in commands
            if(handleBuiltInCommand(proc_list[i])) {
                return;
            }

            // Forking to child process
            pid = fork();
            if(pid < 0) {
                perror("Fork failed");
                exit(-1);
            } else if (pid == 0) { // Child Process
                // Setting up pipes
                if((i < 255) && proc_list[i + 1] != NULL) { // If following process exists
                    outfile = pipefd[i][1]; // Setting output
                } else {
                    outfile = job->stdout;
                }

                closeExtraPipes(pipefd, infile, outfile, job->numProcesses);
                exec_process(proc_list[i], job, job->pgid, infile, outfile, job->stderr, job->is_bg, isInteractive);
            } else { // Parent Process
                proc_list[i]->pid = pid;
                if(job->pgid == -1) {
                    job->pgid = pid;
                }

                setpgid(pid, job->pgid);
            }

            // Closing pipes
            // if(infile != job->stdin) {
            //     close(infile);
            // } else if (outfile != job->stdout) {
            //     close(outfile);
            // }

            infile = pipefd[i][0]; // Setting input
        }        
    }
    closeExtraPipes(pipefd, -1, -1, job->numProcesses);
    
    if(!isInteractive) {
        int status;
        int pid_2;
         do {
            pid_2 = waitpid(-job->pgid, &status, WUNTRACED);
            updateJobStatus(job, pid_2, status);
        } while (!isJobCompleted(job) && !isJobStopped(job));
    } else if(job->is_bg) {
        bg(job, false);
    } else {
        fg(job, false);
    }
}

int main(int argc, char* argv[]) {
    sh_term = STDIN_FILENO;
    FILE* input_stream;
    bool isInteractive;

    // Checking args
    if (argc > 2) {
        perror("Incorrect usage of wsh");
        exit(-1);
    }

    // Checking to run batch file
    if (argc == 1) {
        // Ensuring subshell is the fg proc to implement job control
        while (tcgetpgrp(sh_term) != (sh_pgid = getpgrp())) {
            kill(-sh_pgid, SIGTTIN);
        }

        // Ensuring subshell ignores stop signals
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_IGN);

        // Shell setup
        if (getpid() != getsid(0)) { // The shell is not a session leader
            sh_pgid = getpid();
            if (setpgid(sh_pgid, sh_pgid) < 0) {
                perror("Coudn't set shell pgid");
                exit(-1);
            }
        } 

        // Placing shell in fg
        tcsetpgrp(sh_term, sh_pgid);
        input_stream = stdin;
        isInteractive = true;
    } else {
        input_stream = fopen(argv[1], "r");
        if(input_stream == NULL) {
            perror("Error opening file");
            exit(-1);
        }
        isInteractive = false;
    }

        while (true) {
            // Cleaning job_list and checking for terminated jobs
            cleanJobList();
            
            // Prompting for user input
            char* ibuf = NULL;
            size_t len = 0;
            ssize_t read;
            printf("wsh> ");

            if ((read = getline(&ibuf, &len, input_stream)) ==
                -1) {               // Checking for EOF char
                if (feof(stdin)) {  // Exiting if CTRL-D is pressed
                    exit(0);
                }
                exit(-1);
            }

            struct Job *child_job = malloc(sizeof(struct Job));
            if (hasPipe(ibuf)) {
                parseInputPipes(child_job, ibuf);
            } else {
                parseInput(child_job, ibuf);
            }

            // Running child_job
            exec_job(child_job, isInteractive);
        }
} 