#include "wc.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

typedef struct wordNode {
    char *str;  // the word
    int occurrence;
    struct wordNode *next;  // other word with same hashKey
} wordNode;

struct wc {
    /* you can define this struct to have whatever fields you want. */
    wordNode **hashTable;  //* array of wordNode*
};

int tableSize = 0;  // make hashtale's size global variable

// Description: generate hash key for a word
int myHashFunction(char *str) {
    int hashKey = 0;

    while (*str++ != '\0')
        hashKey = *str + ((hashKey << 4) + hashKey);  // random hash function
    hashKey = (hashKey < 0) ? hashKey * -1 : hashKey;  // make all hashKey positive

    return hashKey % tableSize;  // hashKey < tableSize
}

void insertNode(struct wc *wc, char *str) {
    int hashKey    = myHashFunction(str);
    wordNode *prev = NULL;
    wordNode *curr = wc->hashTable[hashKey];

    // 1° first time occurrence of mapKey
    if (!wc->hashTable[hashKey]) {
        wc->hashTable[hashKey]             = (wordNode *)malloc(sizeof(wordNode));
        wc->hashTable[hashKey]->occurrence = 1;
        wc->hashTable[hashKey]->str        = (char*) malloc((strlen(str) + 1) * sizeof(char));
        strcpy(wc->hashTable[hashKey]->str, str);
        wc->hashTable[hashKey]->next = NULL;
        return;
    }

    // 2° Have checked this string before
    else {
        while (curr) {
            if (!strcmp(str, curr->str)) {  // same string
                curr->occurrence += 1;
                return;
            }
            prev = curr;
            curr = curr->next;
        }

        //  3° Another key for this mapKey
        if (curr == NULL) {  // iterate over the linked list
            // create a new wordNode
            curr             = (wordNode *)malloc(sizeof(wordNode));
            curr->occurrence = 1;
            curr->str        = malloc(strlen(str) + 1);
            strcpy(curr->str, str);
            prev->next = curr;
            curr->next = NULL;
        }
    }

    return;
}

struct wc *wc_init(char *word_array, long size) {
    struct wc *wc;

    wc = (struct wc *)malloc(sizeof(struct wc));
    assert(wc);  // check successful allocation

    int wordCount = 0;

    int i = 0;
    for (; i < size; i++) {
        if (isspace(word_array[i]))
            wordCount++;
    }
    if (word_array[i] != ' ')
        wordCount += 1;  // one more word than previously count

    tableSize = 2 * wordCount;  // good rule of thumb is to use a size that is roughly twice the total number of elements that are expected to be stored in the hash table.


    wc->hashTable = (wordNode **)malloc(tableSize * sizeof(wordNode));

    assert(wc->hashTable);

    for (int i = 0; i < tableSize; i++)
        wc->hashTable[i] = NULL;  // initialize to NULL first

    //* need to duplicate the array
    char *duplicateArray = strdup(word_array);  // Returns a pointer to a null-terminated byte string, which is a duplicate of the string pointed to by str1. The returned pointer must be passed to free to avoid a memory leak.

    char *token = strtok(duplicateArray, " \n\r\t");  // "\n\t\r " is the delimeter
    while (token != NULL) {
        insertNode(wc, token);
        token = strtok(NULL, " \n\r\t");
    }
    free(duplicateArray);

    return wc;
}


void wc_output(struct wc *wc) {
    for (int i = 0; i < tableSize; i++) {
        for (wordNode *wn = wc->hashTable[i]; wn != NULL; wn = wn->next)
            printf("%s:%d\n", wn->str, wn->occurrence);
    }
}

void wc_destroy(struct wc *wc) {
    for (int i = 0; i < tableSize; i++) {
        wordNode *tmp = NULL;
        wordNode *wn  = wc->hashTable[i];
        while (wn != NULL) {
            tmp = wn;
            // tmp is used to keep track of the node to be deleted
            wn = wn->next;  // advance
            free(tmp);
        }
    }
    free(wc->hashTable);
    free(wc);
}

// int main(int argc, char **argv) {
//     struct wc *worldCount = wc_init(argv[1], sizeof(argv[1]));
//     wc_output(worldCount);
//     wc_destroy(worldCount);
// }