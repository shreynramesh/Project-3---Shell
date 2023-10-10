#include <stdbool.h>
#include <stdlib.h>

struct Process {
    pid_t pid;
    char **argv;
    int argc;
    bool completed;
    bool stopped;
    int status;
};

struct Job {
    char *command;
    int id;
    pid_t pgid;
    bool is_bg;
    struct Process *processes[256];
    int stdin, stdout, stderr;
    int numProcesses;
};

extern struct Process *proc_list[256];
extern struct Job *job_list[256];

int sh_term;
pid_t sh_pgid;