struct Process {
    int pid;
    int pjid;
    int argc;
    char* argv[256];
};

struct Job { 
    int id;
    struct Process processes[256];
};

struct JobList {
    struct Job* fg;
    struct Job** bg;
};

struct JobIdList {
    int jobs[256];
};