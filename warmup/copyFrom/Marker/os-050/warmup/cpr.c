#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

void copyFile(char *src, char *dst) {
    // open source file to copy from
    int srcFile = 0, dstFile = 0;
    if ((srcFile = open(src, O_RDONLY)) == -1)
        syserror(open, src);

    // make destination file to copy to
    if ((dstFile = creat(dst, S_IRWXU)) == -1)
        syserror(creat, dst);

    // read source file
    char buf[4096];
    ssize_t readNum  = 0;
    ssize_t writeNum = 0;  // use signed size to handle error or interrupt

    if ((readNum = read(srcFile, buf, 4096)) == -1)
        syserror(read, src); 

    while (readNum > 0) { // if readNum == 0, then empty file
        // write to destination
        writeNum = write(dstFile, buf, readNum);  //* writeNum should be the same as readNum

        if (writeNum < 0)
            syserror(write, dst);

        // continue to read source file
        if ((readNum = read(srcFile, buf, 4096)) == -1)
            syserror(read, src);
    }

    // close source file and destination file
    if (close(srcFile) == -1 || close(dstFile) == -1)
        syserror(close, src);
}

void copyDirectory_R(char *src, char *dst) {
    // get file status in buffer
    struct stat *statBuf = (struct stat *)malloc(sizeof(struct stat));

    if (stat(src, statBuf) == -1)  // get file status failed
        syserror(stat, src);

    if (S_ISREG(statBuf->st_mode)) {  // is a file

        copyFile(src, dst);

        if (chmod(dst, statBuf->st_mode) == -1)  // check if permission is allowed
            syserror(chmod, dst);

    } else if (S_ISDIR(statBuf->st_mode)) {  // is a directory

        // open src directory as directory
        DIR *direc = opendir(src);

        // create dest directory
        if (mkdir(dst, S_IRWXU) == -1)  // error for creating directory
            syserror(mkdir, dst);

        // read direc
        struct dirent *direcRead = (struct dirent *)malloc(sizeof(struct dirent));

        for (direcRead = readdir(direc); direcRead; direcRead = readdir(direc)) {  // end of the directory stream is reached, NULL is returned
            // don't need to copy "." and ".." in Linux
            if (!strcmp(direcRead->d_name, ".")) {
                direcRead = readdir(direc);  // skip .
            } else if (!strcmp(direcRead->d_name, "..")) {
                direcRead = readdir(direc);  // skip ..
            } else {
                // new file name for destination
                char *newDestinationName = (char *)malloc((strlen(direcRead->d_name) + strlen(dst) + 2) * sizeof(char));  // add "/" and "\0"
                strcpy(newDestinationName, dst);
                strcat(newDestinationName, "/");
                strcat(newDestinationName, direcRead->d_name);

                // new file name for source
                char *newSourceName = (char *)malloc((strlen(direcRead->d_name) + strlen(src) + 2) * sizeof(char));  // add "/" and "\0"
                strcpy(newSourceName, src);
                strcat(newSourceName, "/");
                strcat(newSourceName, direcRead->d_name);

                copyDirectory_R(newSourceName, newDestinationName);  // copy recursively

                free(newSourceName);
                free(newDestinationName);
            }
        }

        if (chmod(dst, statBuf->st_mode) == -1)  // check if permission is allowed
            syserror(chmod, dst);

        //close direc
        if(closedir(direc) == -1)
            syserror(closedir, src);

        free(direcRead);
    }

    free(statBuf);
}

// use syserror() when a system call fails

void usage() {
    fprintf(stderr, "Usage: cpr srcdir dstdir\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage();
    }

    copyDirectory_R(argv[1], argv[2]);
    
    return 0;
}
