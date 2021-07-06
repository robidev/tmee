#!/bin/bash

gcc -c -Wall -Werror -fpic main.c
gcc -shared -o template.so main.o

#gcc -fpic -pie -o rt_module_1.so main.c -Wl,-E
