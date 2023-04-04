#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
  char msg[] = "Goodbye World xv6, Goodbye World xv6, Goodbye World xv6, Goodbye World xv6"; //prints at most 32 bits
  exit(0, msg);
}
