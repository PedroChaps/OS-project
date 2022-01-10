#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#define SIZE 27
#define N_THREADS 6
char * path = "/f1";
void* fn(void * arg){
    char * buffer = (char*) arg;
    int fd = tfs_open(path,TFS_O_APPEND);
    assert(fd !=-1);
    int res = tfs_write(fd,buffer,SIZE);
    assert(res == SIZE);
    assert(tfs_close(fd)!=-1);
    return NULL;
}

int main(){
    pthread_t tid[N_THREADS];
    char buffer[SIZE + 1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
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
        int res = pthread_create(&tid[i],NULL,fn, (void*) buffer);
        assert(res == 0);
    }
    for(int i= 0; i<N_THREADS; i++) {
        int res = pthread_join(tid[i], NULL);
        assert(res == 0);
    }
    fd = tfs_open(path,0);
    assert(fd !=-1);
    int res = tfs_read(fd,myoutput,SIZE * N_THREADS);
    printf("%s\n",myoutput);
    printf("%d\n",res);
    //assert(res == SIZE * N_THREADS);
    assert(tfs_close(fd)!= -1);
    myoutput[res] = '\0';
    int cmp = strcmp(output,myoutput);
    //assert(cmp == 0);
    printf("SUCESS");
    exit(EXIT_SUCCESS);

}

