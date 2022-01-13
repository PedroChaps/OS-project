#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#define SIZE 27
#define N_THREADS 20
char * path = "/f1";
void* fn(void * arg){
    char * buffer = (char*) arg;
    int fd = tfs_open(path,TFS_O_APPEND);
    assert(fd !=-1);
    assert(tfs_write(fd,buffer,SIZE) == SIZE);
    assert(tfs_close(fd)!=-1);
    return NULL;
}

int main(){
    pthread_t tid[N_THREADS];
    char *buffer= "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    char output[SIZE*N_THREADS + 1];
    char myoutput[SIZE*N_THREADS + 1];
    int of = 0;
    for (int i = 0; i< N_THREADS; i++){
        memcpy(output + of, buffer, SIZE);
        of += SIZE;
    }
    output[of] = '\0';
    assert(tfs_init()==0);
    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);
    assert(tfs_close(fd)!= -1);
    for (int i = 0; i < N_THREADS; i++ ){
        assert(!pthread_create(&tid[i],NULL,fn, (void*) buffer));
    }
    for(int i= 0; i<N_THREADS; i++) {
        assert(!pthread_join(tid[i], NULL));
    }

    fd = tfs_open(path,0);
    assert(fd !=-1);
    assert(tfs_read(fd,myoutput,SIZE * N_THREADS) == SIZE*N_THREADS);
    assert(tfs_close(fd)!= -1);
    assert(memcmp(output,myoutput,SIZE * N_THREADS)== 0); 
    printf("SUCESS\n");
    exit(EXIT_SUCCESS);

}

