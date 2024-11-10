// File: fib.c
#include <stdio.h>

int _start() {
    int a = 0, b = 1, temp, i;
    for (i = 0; i < 10; i++) {
        printf("Fib(%d) = %d\n", i, a);
        temp = a;
        a = b;
        b = temp + b;
    }
    return 0;
}

