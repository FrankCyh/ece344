#include "wc.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

// Description: Use each wordNode to record the string of the word, the number of occurrence of the word in the input
// Description: words with same hashKey forms a linked list
typedef struct wordNode {
    char *str;  // keep the word
    int occurrence;
    struct wordNode *next;  // other word with same hashKey
} wordNode;

// Description: array of linked list of wordNode*
struct wc {
    wordNode **hashTable;
};

int tableSize = 0;  // make hashtale's size global variable

// Description: insert a new string to the array of linked list
int myHashFunction(char *str) {
    int hashKey = 0;

    while (*str++ != '\0')
        hashKey = *str + ((hashKey << 4) + hashKey);  // random hash function
    hashKey = (hashKey < 0) ? hashKey * -1 : hashKey;  // make all hashKey positive

    return hashKey % tableSize;  // hashKey < tableSize
}

void insertNode(struct wc *wc, char *str) {
    int hashKey = myHashFunction(str);

    // 1° first time occurrence of mapKey
    if (!wc->hashTable[hashKey]) {
        wc->hashTable[hashKey]             = (wordNode *)malloc(sizeof(wordNode));
        wc->hashTable[hashKey]->occurrence = 1;
        wc->hashTable[hashKey]->str        = malloc(strlen(str) + 1);
        strcpy(wc->hashTable[hashKey]->str, str);
        wc->hashTable[hashKey]->next = NULL;
        return;
    }


    wordNode *prev = NULL;
    wordNode *curr = wc->hashTable[hashKey];

    // 2° Have checked this string before
    while (curr) {
        if (strcmp(str, curr->str) == 0) {  // same string
            curr->occurrence += 1;
            return;
        }
        // iterate over the linked list
        prev = curr;
        curr = curr->next;
    }

    //  3° Another key for this mapKey
    if (curr == NULL) {  // dispensable
        // create a new wordNode and attach it to the end of the linked list
        curr      = (wordNode *)malloc(sizeof(wordNode));
        curr->str = malloc(strlen(str) + 1);
        strcpy(curr->str, str);
        curr->occurrence = 1;

        prev->next = curr;
        curr->next = NULL;
    }

    return;
}

struct wc *wc_init(char *word_array, long size) {
    struct wc *wc;

    wc = (struct wc *)malloc(sizeof(struct wc));
    assert(wc);  // check successful allocation

    int wordCount = 0;

    // $ count the number of words
    int i = 0;
    for (; i < size; i++) {
        if (isspace(word_array[i]))
            wordCount++;
    }
    if (word_array[i] != ' ')
        wordCount += 1;  // one more word than previously count

    tableSize = 2 * wordCount;  // good rule of thumb is to use a size that is roughly twice the total number of elements that are expected to be stored in the hash table.

    //$ Initialize the table
    wc->hashTable = (wordNode **)malloc(tableSize * sizeof(wordNode *));
    assert(wc->hashTable);

    for (int i = 0; i < tableSize; i++)
        wc->hashTable[i] = NULL;  // initialize to NULL first

    //* need to duplicate the array
    char *duplicateArray = strdup(word_array);  // Usage: Returns a pointer to a null-terminated byte string, which is a duplicate of the string pointed to by str1. The returned pointer must be passed to free to avoid a memory leak.

    char *token = strtok(duplicateArray, " \t\n\r");  // "\n\t\r " is the delimeter
    while (token != NULL) {
        insertNode(wc, token);
        token = strtok(NULL, " \t\n\r");
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

int main(int argc, char **argv) {
    struct wc *worldCount = wc_init(argv[1], sizeof(argv[1]));
    wc_output(worldCount);
    wc_destroy(worldCount);
}