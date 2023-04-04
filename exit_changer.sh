#!/bin/bash
# Author: Elias Assaf
# place file in xv6 directory and run using 'bash exit_changer.sh' or './exit_changer.sh'
# adds another arguement to all 'exit' calls [exit(0) becomes exit(0,0)], can be easily modified to also work on 'wait'.
# use at your own risk, and absolutely backup everything before using, preferably using git.
# if you want to understand how it works, open vim on some file in the user directory for example 'vim cat.c',
# then manually type ':%s/exit(\([0-9]\+\));/exit(\1,0);/g'
# in kernel directory there is only 2 calls to exit, change them manually.
# Modify all relevant files in the kernel directory
cd /workspaces/xv6-riscv

# Modify all relevant files in the kernel directory
find kernel -name "*.c" -o -name "*.h" | xargs sed -i '' -e 's/\bwait(\([[:digit:]]\+\))/wait(\1, (char*)0)/g'

# Modify all relevant files in the user directory
find user -name "*.c" -o -name "*.h" | xargs sed -i '' -e 's/\bwait(\([[:digit:]]\+\))/wait(\1, (char*)0)/g'


