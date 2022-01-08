#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define COUNT 80
#define SIZE 26
#define N_THREADS 20 //max = 20 : no pior caso as threads abrem todas o ficheiro antes de alguma a fechar, a tablea de ficheiros abertos so têm  20 entradas
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

    char arg [N_THREADS][4];
    for(int i = 0; i< N_THREADS; i++){
        arg[i][0] = '/';
        arg[i][1] = 'f';
        arg[i][2] = i + 1 + '0';
        arg[i][3] = '\0';

    }
    for (int i = 0; i < N_THREADS; i++) {
        int suc = pthread_create(&threads[i], NULL, fn, (void *) arg[i]);
        assert(suc == 0);
    }
    sleep(1);
    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < N_THREADS; i++) {
        int fd = tfs_open(arg[i], 0);
        assert(fd != -1); //se conseguir abrir com esta flag, quer dizer que o ficheiro existe
        assert(tfs_close(fd) != -1);
    }

    printf("Sucessful test\n");

    return 0;
}
