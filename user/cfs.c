#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//A test program that:
//(a) Forks 3 processes with a low, normal and high priority respectively.(0,1,2)
//(b) Each child should perform a simple loop of 1,000,000 iterations, sleeping for 1 second every 100,000 iterations.
//(c) Each child process should then print its PID, CFS priority, run time, sleep time, and runnable time.

void print_stats(int id) {
    //int lock;
    for (int i = 0; i < 1000000; i++) {
        if (i % 100000 == 0) {
            sleep(1);
        }
    }
    int arr[4];
    get_cfs_stats(id, arr);
    printf("proc id: %d, proc cfs priority: %d, proc cfs rtime: %d, proc cfs stime: %d, proc cfs retime: %d\n",id , arr[0], arr[1], arr[2], arr[3]);
    sleep(10); //gives additional time for the output to be printed
}

int main() {
    int child_pid[3];
    for (int i = 0; i < 3; i++) {
        int pid = fork();
        if (pid == -1) {
            exit(1, 0);
        } else if (pid == 0) { //child proc
            set_cfs_priority(i);
            print_stats(getpid());
            exit(0, "");
        }
        else{
            child_pid[i] = pid;
        }
    }
    for (int i = 0; i < 3; i++) {
        wait(&child_pid[i], 0);
    }
    return 0;
}