void exec_job(struct Job* job) {
    pid_t pid;
    int pipefd[2], infile, outfile;

    infile = job->stdin;
    struct Process **proc_list = job->processes;
    for(int i = 0; i < 256; i++) {
        // Setting up pipes
        if(proc_list[i] != NULL) {
            if((i < 255) && proc_list[i + 1] != NULL) {
                if(pipe(pipefd) < 0) {
                    perror("Coudn't create pipe");
                    exit(-1);
                }

                outfile = pipefd[1]; // Setting output
            } else {
                outfile = job->stdout;
            }

            printf("pipein: %d, pipeout: %d, infile: %d, outfile: %d, jobstdin: %d, jobstdout, %d\n", pipefd[0], pipefd[1], infile, outfile, job->stdin, job->stdout);

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
                exec_process(proc_list[i], job, job->pgid, infile, outfile, job->stderr, job->is_bg);
            } else { // Parent Process
                proc_list[i]->pid = pid;
                if(job->pgid == -1) {
                    job->pgid = pid;
                }

                setpgid(pid, job->pgid);
            }

            // Closing pipes
            if(infile != job->stdin) {
                close(infile);
            } else if (outfile != job->stdout) {
                close(outfile);
            }
            infile = pipefd[0]; // Setting input
        }        
    }
    
    if(job->is_bg) {
        bg(job, false);
    } else {
        fg(job, false);
    }
}

void closeExtraPipes(int *pipefd[], int infile, int outfile, int numProcesses) {
    for(int i = 0; i < numProcesses; i++) {
        for(int j = 0; j < 2; j++) {
            if(pipefd[i][j] != infile && pipefd[i][j] != outfile) {
                printf("Closing %d", pipefd[i][j]);
                close(pipefd[i][j]);
            }
        }
    }
}

void exec_job(struct Job* job) {
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
    for(int i = 0; i < numProcesses; i++) {
        
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

                printf("infile: %d, outfile: %d, jobstdin: %d, jobstdout, %d\n", infile, outfile, job->stdin, job->stdout);
                exec_process(proc_list[i], job, job->pgid, infile, outfile, job->stderr, job->is_bg);
            } else { // Parent Process
                proc_list[i]->pid = pid;
                if(job->pgid == -1) {
                    job->pgid = pid;
                }

                setpgid(pid, job->pgid);
            }

            infile = pipefd[i][0]; // Setting input
        }        
    }
    closeExtraPipes(pipefd, -1, -1, job->numProcesses);
    
    if(job->is_bg) {
        bg(job, false);
    } else {
        fg(job, false);
    }
}