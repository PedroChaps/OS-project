#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 27
#define N_THREADS 20 //max 20

/**
   This test uses multiple threads to write on the same file (and same fh) and checks whether the result
   was the correct one.
   A maximum of N_THREADS = 10476 can be used (if the value is exceeded, an error will occur because
   it exceeds the file size.
   N_THREADS threads are created and each writes the abecedary to the file, by calling the function
   `tfs_write`.
 */
char *path = "/f1";

void* fn_write(void* arg){
    char* buffer = (char*) arg;
    int fd = tfs_open(path, TFS_O_TRUNC);
    assert(fd!=-1);
    assert(tfs_write(fd,buffer,11)==11);
    assert(tfs_close(fd) == 0);
    return NULL;
}


void* fn_read(void* arg){
    char* buffer = (char*) arg;
    int fd = tfs_open(path, 0);
    assert(fd!=-1);
    ssize_t size = tfs_read(fd,buffer,SIZE);
    buffer[size] = '\0';
    assert(size == SIZE || size == 11);
    assert(!tfs_close(fd));
    return NULL;
}

int main() {
    pthread_t write_thread;
    pthread_t threads[N_THREADS-1];
    
    assert(tfs_init() == 0);
    /* Writing this buffer multiple times to a file stored on 1KB blocks will
       always hit a single block (since 1KB is a multiple of SIZE=256) */

    char* write = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    char* thread_write = "ola ola ola";
    
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd!=-1);
    assert(tfs_write(fd,write,SIZE) == SIZE);
    assert(tfs_close(fd) == 0);
    char outputs[N_THREADS-1][SIZE + 1];

    for (int ix = 0; ix < N_THREADS; ix++){
        if(ix == 0){
            assert(!pthread_create(&write_thread, NULL,fn_write,(void*)thread_write));
        }
        else{
            assert(!pthread_create(&threads[ix-1],NULL,fn_read,(void*)outputs[ix-1]));
        }
    }

    for(int ix = 0; ix < N_THREADS; ix++){
        if(ix == 0)
            assert(!pthread_join(write_thread,NULL));
        else
            assert(!pthread_join(threads[ix-1],NULL));
    }

    for (int ix = 0; ix < N_THREADS-1; ix++){
        int res1 = strcmp(write,outputs[ix]);
        int res2 = strcmp(thread_write,outputs[ix]);
        int res3 = res1 & res2;
        assert(res3 == 0);
    }
    
    printf("Sucessful test\n");

    return 0;
}