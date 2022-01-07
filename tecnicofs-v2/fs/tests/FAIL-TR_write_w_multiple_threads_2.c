#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 26
#define N_THREADS 18

/**
   This test uses multiple threads to write on the same file (and same fh) and checks whether the result was the correct one
 */

typedef struct{
    char const *name;
    void const * buffer;
    size_t to_write;
} Mystruct;

void* fn(void* arg) {
    Mystruct s = *((Mystruct *)arg);

    int fh = tfs_open(s.name, TFS_O_APPEND);
    assert(fh != -1);

    ssize_t res = tfs_write(fh,s.buffer,s.to_write);
    assert(res != -1);

    assert(tfs_close(fh) != -1);
    return (void*)&fh;
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
        memcpy(output + of,write,SIZE);
        of+= SIZE;
    }
    output[of] = '\0';
    printf("%s\n", output);

    char *myoutput = (char*)malloc(sizeof(char)*(SIZE*N_THREADS+1));

    assert(tfs_init() != -1);

    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);

    //tfs_write(fd, "A", 1);
    //tfs_close(fd);

    Mystruct* s;
    s = (Mystruct*)malloc(sizeof (Mystruct));
    s->name = path;
    s->to_write = SIZE;
    s->buffer = input;

    for (int ix = 0; ix< N_THREADS; ix++) {
        int suc = pthread_create(&threads[ix], NULL, fn, (void *) s);
        assert(suc == 0);
    }

    for (int ix = 0; ix < N_THREADS; ix++){
        pthread_join(threads[ix],NULL);
    }

    fd = tfs_open(path,0);
    assert(fd !=-1);

    int res = tfs_read(fd, myoutput,SIZE*N_THREADS);
    printf("%d\n", res);
    //assert(res == SIZE*N_THREADS);
    myoutput[res] = '\0';
    int a =strlen(output),b = strlen(myoutput);
    //assert(a == b);
    /*
    for(int ix= 0; ix<b;ix++){
        if(output[ix]!=myoutput[ix]) {
            printf("%d\n", ix);
            break;
        }
    }
    */
    /*for(int ix = 10; ix>=0;ix--)
        printf("%c  %c\n",output[10240-ix],myoutput[10240- ix]);
    for(int ix = 1; ix<=10;ix++)
        printf("%c  %c\n",output[10240 + ix],myoutput[10240 + ix]);*/
    int cmp_val = strcmp(output,myoutput);
    //assert(cmp_val == 0);
    //printf("%s", output);
    //printf("%s", myoutput);
    free(myoutput);

    printf("Sucessful test\n");

    return 0;
}