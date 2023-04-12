#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//A test program that:
//(a) Forks 3 processes with a low, normal and high priority respectively.(0,1,2)
//(b) Each child should perform a simple loop of 1,000,000 iterations, sleeping for 1 second every 100,000 iterations.
//(c) Each child process should then print its PID, CFS priority, run time, sleep time, and runnable time.

void print_stats(int id) {
    int i;
    for (i = 0; i < 100000000; i++) { //we added two 0's in the loop and the if do get more valuable stats
        if (i % 10000000 == 0) {
            sleep(1);
        }
    }
    int arr[4];
    get_cfs_stats(id, arr, 1); //printing is done in kernel space as a work around to avoid the processes interfering each other's prints
}

int main() {
    int child_pid[6];
    for (int i = 0; i < 6; i++) {
        int pid = fork();
        if (pid == -1) {
            exit(1, "");
        } else if (pid == 0) { //child proc
            set_cfs_priority(i);
            print_stats(getpid());
            exit(0, "");
        }
        else{
            child_pid[i] = pid;
        }
    }
    for (int i = 0; i < 6; i++) {
        wait(&child_pid[i], 0);
    }
    return 0;
}