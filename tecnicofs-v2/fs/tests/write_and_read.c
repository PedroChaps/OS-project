#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define COUNT 80
#define SIZE 27
#define N_THREADS 8000

int main(){

    char *path = "/f1";
    assert(tfs_init()!=-1);
    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd!=-1);
    char sing_input[SIZE+1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    char input[SIZE*N_THREADS];
    char output[SIZE*N_THREADS];
    int of = 0;
    for (int ix = 0;ix<N_THREADS;ix++){
        memcpy(input+of,sing_input,SIZE);
        of += SIZE;
    }
    assert(tfs_write(fd,input,SIZE*N_THREADS)==SIZE*N_THREADS);
    assert(tfs_close(fd)!=-1);
    fd = tfs_open(path,0);
    assert(fd!=-1);
    assert(tfs_read(fd,output,SIZE*N_THREADS) == SIZE*N_THREADS);
    assert(memcmp(output,input,SIZE*N_THREADS) == 0);
    printf("SUCCESS\n");
    return 0;

}

