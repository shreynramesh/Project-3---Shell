struct Process {
    pid_t pid;
    int pjid;
    pid_t pgid;
    int argc;
    char *argv[256];
};

enum ProcessStatus {
    RUNNING,
    STOPPED,
};

struct Job {
    int jid;
    int pgid;
    int fg;
    enum ProcessStatus status;
    struct Process processes[256];
};