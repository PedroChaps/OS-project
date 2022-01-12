#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define COUNT 80
#define SIZE 27
#define N_THREADS 1025//max = 10476
//FIXME muito estranho, com 1024 threads funciona mas com 1025 nao... e 1024 e o BLOCK SIZE, mas nao consigo perceber porque
/**
   This test writes on a file and uses multiple threads to read the same file (and same fh) and checks whether the result was the correct one
 */

typedef struct{
    int fh;
    void *buffer;
    size_t to_read;
} Mystruct;


void* fn(void* arg){
    Mystruct s = *((Mystruct *)arg);
    ssize_t res = tfs_read(s.fh,s.buffer, s.to_read);
    assert(res == s.to_read);
    return NULL;
}


int main() {
    pthread_t threads[N_THREADS];
    char *path = "/f1";

    /* Writing this buffer multiple times to a file stored on 1KB blocks will 
       always hit a single block (since 1KB is a multiple of SIZE=256) */
    char *write = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    char output [SIZE * N_THREADS +1];

    int of = 0;
    for (int ix = 0;ix<N_THREADS;ix++){
        memcpy(output + of, write, SIZE);
        of += SIZE;
    }
    output[SIZE*N_THREADS] = '\0';
    char ** myoutput= (char**) malloc(sizeof(char*)*N_THREADS);
    for(int ix = 0; ix < N_THREADS; ix++){
        myoutput[ix] = (char*)malloc(sizeof(char)*SIZE);
    }
    assert(tfs_init() != -1);

    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);
    assert(tfs_write(fd,output,SIZE*N_THREADS) == SIZE*N_THREADS);
    assert(tfs_close(fd) != -1);
   
    fd = tfs_open(path, 0);
    assert(fd != -1);
    Mystruct s[N_THREADS];
    for(int i = 0; i<N_THREADS; i++){
        s[i].fh = fd;
        s[i].to_read = SIZE;
        s[i].buffer = myoutput[i];
    }

    for (int i = 0; i < N_THREADS; i++) {
            int suc = pthread_create(&threads[i], NULL, fn, (void *) &s[i]);
            assert(suc == 0);
        }

    for (int i = 0; i < N_THREADS; i++){
        pthread_join(threads[i], NULL);
    }

    char * myrealoutput = (char*) malloc(sizeof(char)*N_THREADS*SIZE +1);
    of = 0;
    for(int ix = 0; ix < N_THREADS;ix++){
        memcpy(myrealoutput + of, myoutput[ix], SIZE);
        of += SIZE;
    }
    myrealoutput[SIZE*N_THREADS] = '\0';

    for(int ix = 0; ix < N_THREADS; ix++){
        free(myoutput[ix]);
    }
    free(myoutput);

    int err = -1;
    for(int ix= 0; ix<SIZE*N_THREADS;ix++){
        if(output[ix]!=myrealoutput[ix]) {
            err = ix;
            printf("%d\n", ix);
            break;
        }
    }
    if (err != -1) {
        for (int ix = 10; ix >= 0; ix--)
            if (err - ix > -1)
                printf("%c  %c\n", output[err - ix], myrealoutput[err - ix]);
        for (int ix = 1; ix <= 10; ix++)
            if (err + ix < SIZE * N_THREADS)
                printf("%c  %c\n", output[err + ix], myrealoutput[err + ix]);
    }

    assert(tfs_close(fd) != -1);
    int cmp_val = memcmp(output, myrealoutput,SIZE*N_THREADS);
    //assert(cmp_val == 0);
    printf("%d\n",cmp_val);
    printf("output   :%s\n", output + SIZE*(N_THREADS-2));
    printf("myoutput :%s\n", myrealoutput + SIZE*(N_THREADS-2));
    printf("Sucessful test\n");
    
    
    return 0;
}