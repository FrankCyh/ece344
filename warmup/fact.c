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
        printf("Huh?\n");
    else {
        char* endPtr;
        double val = strtod(argv[1], &endPtr);  // record the last position of string recorded

        if (endPtr == argv[1])  // not a double, is a sting; e.g. "hello"
            printf("Huh?\n");

        else if (!isInteger(val) || val <= 0)  // if val is not an integer or if first argument passed is not positive
            printf("Huh?\n");

        else if (val > 12)  // if overflow
            printf("Overflow\n");

        else
            printf("%d\n", factorial(val));
    }
    return 0;
}
