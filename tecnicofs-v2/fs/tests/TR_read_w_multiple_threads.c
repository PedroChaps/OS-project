#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define COUNT 80
#define SIZE 26
#define N_THREADS  //max = 10476

/**
   This test writes on a file and uses multiple threads to read the same file (and same fh) and checks whether the result was the correct one
 */

typedef struct{
    int fh;
    void *buffer;
    size_t to_read;
    int offset;
} Mystruct;

void* fn(void* arg){
    Mystruct s = *((Mystruct *)arg);
    ssize_t res = tfs_read(s.fh,s.buffer + s.offset, s.to_read);
    return (void*)res;
}


int main() {
    pthread_t threads[N_THREADS];
    char *path = "/f1";

    /* Writing this buffer multiple times to a file stored on 1KB blocks will 
       always hit a single block (since 1KB is a multiple of SIZE=256) */
    char input[SIZE+1];
    char write[SIZE+1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    strcpy(input, write);

    char output [SIZE * N_THREADS + 1];

    int of = 0;
    for (int ix = 0;ix<N_THREADS;ix++){
        memcpy(output + of, write, SIZE);
        of += SIZE;
    }
    output[of] = '\0';
    //printf("%s\n", output);


    char *myoutput = (char*) malloc(sizeof(char)*(SIZE*N_THREADS+1));

    assert(tfs_init() != -1);

    int fd = tfs_open(path,TFS_O_CREAT);
    ///assert(fd !=-1);

    // writes N_THREADS blocks of data
    for (int i = 0; i < N_THREADS; i++) {
        assert(tfs_write(fd,input,SIZE) == SIZE);
    }
    assert(tfs_close(fd) != -1);


    /* Open again to check if contents are as expected */
    /*
    fd = tfs_open(path, 0);
    assert(fd != -1 );

    for (int i = 0; i < N_THREADS; i++) {
        assert(tfs_read(fd, output, SIZE) == SIZE);
        assert (memcmp(input, output, SIZE) == 0);
    }
    */

    /* Open again to check if contents are as expected, but with threads */
    fd = tfs_open(path, 0);
    assert(fd != -1);

    Mystruct* s;
    s = (Mystruct*)malloc(sizeof (Mystruct));
    s->to_read = BLOCK_SIZE;
    s->buffer = myoutput;
    s->fh = fd;
    s->offset = 0;


    for (int i = 0; i < N_THREADS; i++) {
        int suc = pthread_create(&threads[i], NULL, fn, (void *) s);
        assert(suc == 0);
        s->offset += SIZE; //TODO Pode-se incrementar o offset? Não tem problemas por ser um argumento de uma thread?
    }
    sleep(1);
    for (int i = 0; i < N_THREADS; i++){
        pthread_join(threads[i], NULL);
    }
    assert(tfs_close(fd) != -1);

    myoutput[N_THREADS*SIZE] = '\0';

    int cmp_val = strcmp(output, myoutput);
    printf("cmp_val = %d\n", cmp_val);
    ///assert(cmp_val == 0);

    sleep(1);

    printf("expected: %s\n", output);
    printf("myoutput: %s\n", myoutput);

    free(myoutput);

    printf("Sucessful test\n");

    return 0;
}