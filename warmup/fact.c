#include <stdbool.h>

#include "common.h"

int factorial(int n) {
    if (n == 1)
        return 1;
    return n * factorial(n - 1);
}

bool isInteger(double a) {
    int b = (int)a;
    return b == a;
}

int main(int argc, char** argv) {
    if (argc == 1)  // if no argument is passed in
        printf("Huh?");
    else {
        char* endPtr;
        double val = strtod(argv[1], &endPtr); // record the last position of string recorded

        if (!isInteger(val))  // if val is not an integer
            printf("Huh?");

        else if (val > 12 || val <= 0)  // if first argument passed is not positive
            printf("Overflow");

        else
            printf("%d", factorial(val));
    }
    return 0;
}
