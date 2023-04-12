#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int policy = -1;
  policy = atoi(argv[1]); 
  set_policy(policy);
  return 0;
}