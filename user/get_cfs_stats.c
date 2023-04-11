#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{ 
  int arr[4];
  int pid = atoi(argv[1]);  
  get_cfs_stats(pid,arr, 0);
  //printf("%d\n", 2);
  return 0;
}