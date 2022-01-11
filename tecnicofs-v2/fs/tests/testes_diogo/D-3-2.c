#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


#define MAX_SIZE INODE_SIZE_AVAILABLE
#define READ_SIZE 1000

void* fn(void* arg);

int main() {

    pthread_t tid1;
    pthread_t tid2;
    pthread_t tid3;

    char buffer[MAX_SIZE];
    memset(buffer, 'a', MAX_SIZE);

    int file = tfs_open("/testfile", TFS_O_CREAT);
    assert(file != -1);

    ssize_t r = tfs_write(buffer, file, MAX_SIZE);
    assert(r == MAX_SIZE);

    if (pthread_create(&tid1, NULL, fn, NULL) != 0) {
        fprintf(stderr, "Error creating thread 1\n");
    }

    if (pthread_create(&tid2, NULL, fn, NULL) != 0) {
        fprintf(stderr, "Error creating thread 2\n");
    }

    if (pthread_create(&tid3, NULL, fn, NULL) != 0) {
        fprintf(stderr, "Error creating thread 3\n");
    }


    if (pthread_join(tid1, NULL) != 0) {
        fprintf(stderr, "Error in thread 1\n");
    }

    if (pthread_join(tid2, NULL) != 0) {
        fprintf(stderr, "Error in thread 2\n");
    }

    if (pthread_join(tid3, NULL) != 0) {
        fprintf(stderr, "Error in thread 3\n");
    }


    printf("Test not ready.\n");

    return 0;
}

void* fn(void* arg) {
    int file = tfs_open("/testfile", TFS_O_CREAT);
    assert(file != -1);

    char buffer[READ_SIZE];
    char bufferAns[READ_SIZE];

    memset(bufferAns,'a', READ_SIZE);

    ssize_t r = tfs_read(file, buffer, READ_SIZE);

    asset(r == READ_SIZE);
    assert(memcmp(buffer, bufferAns, READ_SIZE) == 0);

    tfs_close(file);

    // So the compiler does not complain about not using the argument
    if (1) {file = *(int*)arg;} 

    return NULL;
}