#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
  char msg[] = "Hello World xv6";
  printf("%s\n", msg); 
  exit(0, "");
}
