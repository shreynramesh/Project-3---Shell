#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wsh.h"

void parseInput(struct Process* process, char* ibuf) {
    char* arg;
    int i = 0; // arg index

    // char* ibuf_copy = strdup(ibuf);
    // if (ibuf_copy == NULL) {
    //     perror("Memory allocation error");
    //     exit(-1);
    // }

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
}

int main(int argc, char *argv[]) {
    while (true) {
        // Prompting for user input
        char *ibuf = NULL;
        size_t len = 0;
        ssize_t read;
        printf("wsh> ");
        if((read = getline(&ibuf, &len, stdin)) == -1) { // Checking for EOF char
            if(feof(stdin)) {
                exit(0);
            }
            exit(-1);
        }

        // Exit shell loop if user types exit or CTRL-D is pressed
        if (strcmp(ibuf, "exit\n") == 0) {
            exit(0);
        }

        // Parsing input for command and arguments
        struct Process* process = malloc(sizeof(struct Process));
        parseInput(process, ibuf);
        for(int i = 0; i < process->argc; i++) {
            printf("%s\n", process->argv[i]);
        }

        // Freeing resources
        free(process); // user process input
        free(ibuf);  // input buffer
    }
}