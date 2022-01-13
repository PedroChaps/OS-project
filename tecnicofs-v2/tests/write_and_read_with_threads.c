#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE1 27
#define SIZE2 11
#define N_THREADS 20

/**
   This test uses multiple threads to read and write at the same time.
   A maximum of N_THREADS = 20 can be used (if the value is exceeded, an error will occur because it exceeds
   the maximum number of file descriptors the TFS can have).
   A simple write, without threads, is made and then, N_THREADS are created.
   The first thread writes a buffer to the TFS and the rest read multiple descriptors at the same time.

   This test has two purposes:
   - Show that rwlocks are used (instead of mutexes), because multiple reads happen while
     something is being written;
   - Read and Write are synchronized, because the value read in each thread is either the first (sequential)
     or second (threaded) write, and not a mix of the first and second write.
 */

/* Global path, for simplification */
char *path = "/f1";

/* Function used in the write thread */
void* fn_write(void* arg){
    char* buffer = (char*) arg;
    /* Opens the file in truncate mode, writes and then closes */
    int fd = tfs_open(path, TFS_O_TRUNC);
    assert(fd!=-1);
    assert(tfs_write(fd, buffer, SIZE2) == SIZE2);
    assert(tfs_close(fd) == 0);
    return NULL;
}

/* Function used in the read threads */
void* fn_read(void* arg){
    char* buffer = (char*) arg;
    /* Opens the file in read mode, reads and then closes */
    int fd = tfs_open(path, 0);
    assert(fd != -1);
    ssize_t size = tfs_read(fd, buffer, SIZE1);
    buffer[size] = '\0';
    /* The size is either the first or second write */
    assert(size == SIZE1 || size == SIZE2);
    assert(!tfs_close(fd));
    return NULL;
}


int main() {
    pthread_t write_thread;
    pthread_t threads[N_THREADS-1];

    /* First (sequential) write and second write (used in a thread) */
    char* write = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    char* thread_write = "ola ola ola";

    /* Initializes the TFS */
    assert(tfs_init() != -1);

    /* Makes the first (sequential) write */
    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd!=-1);
    assert(tfs_write(fd,write,SIZE1) == SIZE1);
    assert(tfs_close(fd) == 0);

    /* Array that stores the outputs of each write */
    char outputs[N_THREADS-1][SIZE1+1];

    /* Iterates over the threads. If it is the first one, writes, else reads */
    for (int i = 0; i < N_THREADS; i++){
        if(i == 0)
            assert(pthread_create(&write_thread, NULL,fn_write, (void*)thread_write) != -1);

        else
            assert(pthread_create(&threads[i-1], NULL, fn_read, (void*)outputs[i-1]) != -1);
    }

    /* Waits for all threads to finish */
    for(int i = 0; i < N_THREADS; i++){
        if(i == 0)
            assert(!pthread_join(write_thread,NULL));
        else
            assert(!pthread_join(threads[i-1],NULL));
    }

    /* Compares the outputs to either values written. */
    for (int i = 0; i < N_THREADS-1; i++){
        int res1 = strcmp(write,outputs[i]);
        int res2 = strcmp(thread_write,outputs[i]);
        int res3 = res1 & res2;
        assert(res3 == 0);
    }
    
    printf("Sucessful test\n");

    return 0;
}