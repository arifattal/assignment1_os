
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void){
    int p_size = memsize();
    printf("the size of the current running proccess is: %d bytes\n",p_size);

    // (b) Allocate 20k more bytes of memory by calling malloc().
    void *array = malloc(4);
    // (c) Print how many bytes of memory the running process is using after the allocation.
    printf("the size of the current running proccess after array allocation is: %d bytes\n",memsize());

    // (d) Free the allocated array.
    free(array);

    // (e) Print how many bytes of memory the running process is using after the release.
    printf("the size of the current running proccess after freeing the array is: %d bytes\n",memsize());

    exit(0, 0);
    return 0;
}