#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <stdbool.h>
#include "helper.h"

pid_t pids[N_PROCS];                        // The pid of each process
FILE *debug, *errors;                       // File descriptors for the two log files
bool status[N_PROCS];                       // Used to check the response status for each process

// Function to get the current time as a string
void get_current_time(char *buffer, int len) {
    time_t now = time(NULL);
    strftime(buffer, len, "%Y-%m-%d %H:%M:%S", localtime(&now));
}
// Kill all processes
void kill_processes() {
    for (int i = 0; i < N_PROCS; i++) {
        if (kill(pids[i], SIGUSR2) == -1) {
            perror("[WATCHDOG]: Error sending signal SIGUSR2 kill from the watchdog to a process, check the error log file to see what it is");
            switch (i) {
                case 0:
                    LOG_TO_FILE(errors, "Error sending signal SIGUSR2 kill to the SERVER");
                    break;
                case 1:
                    LOG_TO_FILE(errors, "Error sending signal SIGUSR2 kill to the DRONE");
                    break;
                case 2:
                    LOG_TO_FILE(errors, "Error sending signal SIGUSR2 kill to the OBSTACLE");
                    break;
                case 3:
                    LOG_TO_FILE(errors, "Error sending signal SIGUSR2 kill to the TARGET");
                    break;
                case 4:
                    LOG_TO_FILE(errors, "Error sending signal SIGUSR2 kill to the INPUT");
                    break;
            }
        }
    }
}

void signal_handler(int sig, siginfo_t* info, void *context) {
    if (sig == SIGUSR1) {
        pid_t pid = info->si_pid;
        for (int i = 0; i < N_PROCS; i++) {
            if (pid == pids[i]) {
                switch (i) {
                    case 0:
                        LOG_TO_FILE(debug, "Received the signal from the SERVER");
                        break;
                    case 1:
                        LOG_TO_FILE(debug, "Received the signal from the DRONE");
                        break;
                    case 2:
                        LOG_TO_FILE(debug, "Received the signal from the OBSTACLE");
                        break;
                    case 3:
                        LOG_TO_FILE(debug, "Received the signal from the TARGET");
                        break;
                    case 4:
                        LOG_TO_FILE(debug, "Received the signal from the INPUT");
                        break;
                }
                status[i] = true;
            }
        }
    }

    if (sig == SIGUSR2) {
        LOG_TO_FILE(debug, "The keyboard manager has sent the termination signal, shutting down the drone and the server");
        printf("Watchdog shutting down by the terminate character: %d\n", getpid());
        kill_processes();
        // Close the files
        fclose(debug);
        fclose(errors);
        exit(EXIT_SUCCESS);
    }
    if (sig == SIGTERM) {
        LOG_TO_FILE(debug, "ENDED");
    }
}

void watchdog(int timeout) {
    char message[256], current_time[32];

    time_t start, finish;
    double diff; 
    while (1) {
        for (int i = 0; i < N_PROCS; i++) {
            status[i] = false;
            if (kill(pids[i], SIGUSR1) == -1) {
                perror("[WATCHDOG]: Error sending signal SIGUSR1 kill from the watchdog to a process, check the error log file to see what it is");
                kill_processes();
                switch (i) {
                    case 0:
                        LOG_TO_FILE(errors, "Error sending signal SIGUSR1 kill to the SERVER");
                        break;
                    case 1:
                        LOG_TO_FILE(errors, "Error sending signal SIGUSR1 kill to the DRONE");
                        break;
                    case 2:
                        LOG_TO_FILE(errors, "Error sending signal SIGUSR1 kill to the OBSTACLE");
                        break;
                    case 3:
                        LOG_TO_FILE(errors, "Error sending signal SIGUSR1 kill to the TARGET");
                        break;
                    case 4:
                        LOG_TO_FILE(errors, "Error sending signal SIGUSR1 kill to the INPUT");
                        break;
                }
                printf("Watchdog shutting down: %d\n", getpid());
                // Close the files
                fclose(debug);
                fclose(errors);
                exit(EXIT_FAILURE);
            }
            /**
             * Wait half a second so that the process has time to send the signal to update 
             * the last activity to the watchdog
            */
            time(&start);
            do {
                time(&finish);
                diff = difftime(finish, start);
            } while (diff < 0.5);
        }

        // Wait TIMEOUT second before checking the processes' activities
        time(&start);
        do {
            time(&finish);
            diff = difftime(finish, start);
        } while (diff < TIMEOUT);

        for (int i = 0; i < N_PROCS; i++) {
            if (!status[i]) {
                get_current_time(current_time, sizeof(current_time));
                switch (i) {
                    case 0:
                        snprintf(message, sizeof(message), "The SERVER process [%d] did not respond, or its last activity exceeded the timeout at %s", pids[i], current_time);
                        LOG_TO_FILE(debug, message);
                        break;
                    case 1:
                        snprintf(message, sizeof(message), "The DRONE process [%d] did not respond, or its last activity exceeded the timeout at %s", pids[i], current_time);
                        LOG_TO_FILE(debug, message);
                        break;
                    case 2:
                        snprintf(message, sizeof(message), "The OBSTACLE process [%d] did not respond, or its last activity exceeded the timeout at %s", pids[i], current_time);
                        LOG_TO_FILE(debug, message);
                        break;
                    case 3:
                        snprintf(message, sizeof(message), "The TARGET process [%d] did not respond, or its last activity exceeded the timeout at %s", pids[i], current_time);
                        LOG_TO_FILE(debug, message);
                        break;
                    case 4:
                        snprintf(message, sizeof(message), "The INPUT process [%d] did not respond, or its last activity exceeded the timeout at %s", pids[i], current_time);
                        LOG_TO_FILE(debug, message);
                        break;
                }
                kill_processes();
                printf("Watchdog shutting down: %d\n", getpid());
                // Close the files
                fclose(debug);
                fclose(errors);
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    /* OPEN THE LOG FILES */
    debug = fopen("debug.log", "a");
    if (debug == NULL) {
        perror("[WATCHDOG]: Error opening the debug file");
        kill_processes();
        exit(EXIT_FAILURE);
    }
    errors = fopen("errors.log", "a");
    if (errors == NULL) {
        perror("[WATCHDOG]: Error errors the debug file");
        kill_processes();
        exit(EXIT_FAILURE);
    }

    if (argc != N_PROCS + 1) {
        LOG_TO_FILE(errors, "Invalid number of parameters");
        // Close the files
        fclose(debug);
        fclose(errors);
        exit(EXIT_FAILURE);
    }

    LOG_TO_FILE(debug, "Process started");

    /* Opens the semaphore for child process synchronization */
    sem_t *exec_sem = sem_open("/exec_semaphore", 0);
    if (exec_sem == SEM_FAILED) {
        perror("[WATCHDOG]: Failed to open the semaphore for the exec");
        LOG_TO_FILE(errors, "Failed to open the semaphore for the exec");
        exit(EXIT_FAILURE);
    }
    sem_post(exec_sem); // Releases the resource to proceed with the launch of other child processes

    /* SAVED THE CHILD PIDS */
    for (int i = 0; i < N_PROCS; i++) {
        pids[i] = atoi(argv[i+1]);
    }

    /* SETTING THE SIGNALS */
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);

    // Set the signal handler for SIGUSR1
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("[WATCHDOG]: Error in sigaction(SIGURS1)");
        LOG_TO_FILE(errors, "Error in sigaction(SIGURS1)");
        kill_processes();
        // Close the files
        fclose(debug);
        fclose(errors);
        exit(EXIT_FAILURE);
    }
    // Set the signal handler for SIGUSR2
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("[WATCHDOG]: Error in sigaction(SIGURS2)");
        kill_processes();
        LOG_TO_FILE(errors, "Error in sigaction(SIGURS2)");
        // Close the files
        fclose(debug);
        fclose(errors);
        exit(EXIT_FAILURE);
    }
    // Set the signal handler for SIGTERM
    if(sigaction(SIGTERM, &sa, NULL) == -1){
        perror("[WATCHDOG]: Error in sigaction(SIGTERM)");
        LOG_TO_FILE(errors, "Error in sigaction(SIGTERM)");
        // Close the files
        fclose(debug);
        fclose(errors);
        exit(EXIT_FAILURE);
    }

    // Add sigmask to block all signals execpt SIGURS1, SIGURS2 and SIGTERM
    sigset_t sigset;
    sigfillset(&sigset);
    sigdelset(&sigset, SIGUSR1);
    sigdelset(&sigset, SIGUSR2);
    sigdelset(&sigset, SIGTERM);
    sigprocmask(SIG_SETMASK, &sigset, NULL);

    /* LAUNCH THE WATCHDOG */
    watchdog(TIMEOUT);

    /* END THE PROGRAM */
    // Close the files
    fclose(debug);
    fclose(errors);
    
    return 0;
}