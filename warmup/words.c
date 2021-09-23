#include "common.h"

int main(int argc, char **argv) {
    if (argc == 1)
        printf("\nNo Extra Argument Passed Other Than Program Name");
    if (argc >= 2) 
        for (int i = 1; i < argc; i++)
            printf("%s\n", argv[i]);
    return 0;
}
