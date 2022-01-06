#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 4
#define N_THREADS 300

/**
   This test uses multiple threads to write on the same file (and same fh) and checks whether the result was the correct one
 */

typedef struct{
    int fh;
    void const * buffer;
    size_t to_write;
}Mystruct;

void* fn(void* arg){
    Mystruct s = *((Mystruct *)arg);
    ssize_t res = tfs_write(s.fh,s.buffer,s.to_write);
    return (void*)res;
}


int main() {
    pthread_t threads[N_THREADS];
    char *path = "/f1";

    /* Writing this buffer multiple times to a file stored on 1KB blocks will 
       always hit a single block (since 1KB is a multiple of SIZE=256) */
    char input[SIZE+1];
    char write[SIZE] = "ABCD";
    strcpy(input, write);

    char output [SIZE * N_THREADS + 1];
    char *myoutput = (char*)malloc(sizeof(char)*(SIZE*N_THREADS+1));
    assert(tfs_init() != -1);
    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);
    Mystruct* s;
    s = (Mystruct*)malloc(sizeof (Mystruct));
    s->to_write = SIZE;
    s->buffer = input;
    s->fh = fd;
    for (int ix = 0; ix< N_THREADS; ix++) {
        int suc = pthread_create(&threads[ix], NULL, fn, (void *) s);
        assert(suc == 0);
    }
    for (int ix = 0; ix < N_THREADS; ix++){
        pthread_join(threads[ix],NULL);
    }
    int of = 0;
    for (int ix = 0;ix<N_THREADS;ix++){
        memcpy(output + of,write,SIZE);
        of+= SIZE;
    }
    output[of] = '\0';
    int res = tfs_read(fd,myoutput,SIZE*N_THREADS+1);
    assert(res == SIZE*N_THREADS+1);
    assert(strcmp(output,myoutput) == 0);
    free(myoutput);

    printf("Sucessful test\n");

    return 0;
}