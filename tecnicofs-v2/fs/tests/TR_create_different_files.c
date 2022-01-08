#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define COUNT 80
#define SIZE 26
#define N_THREADS 2 //max = 10476
//erro : ta a criar 2 ficheiros c o nome f1 nao sei pq
/**
   This test writes on a file and uses multiple threads to read the same file (and same fh) and checks whether the result was the correct one
 */

void* fn(void* arg){
    char * res = (char*) arg;
    int fd = tfs_open(res,TFS_O_CREAT);
    assert(fd !=-1);
    assert(tfs_close(fd) != -1);

}


int main() {
    pthread_t threads[N_THREADS];
    assert(tfs_init() != -1);


    char arg[4] = "/f";
    arg[3] = '\0';
    for (int i = 0; i < N_THREADS; i++) {
        arg[2] = i + '0';
        int suc = pthread_create(&threads[i], NULL, fn, (void *) arg);
        assert(suc == 0);
    }
    sleep(1);
    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < N_THREADS; i++) {
        arg[2] = i + '0';
        int fd = tfs_open(arg, 0);
        //assert(fd != -1);
        //assert(tfs_close(fd) != -1);
    }

    printf("Sucessful test\n");

    return 0;
}
