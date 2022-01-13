#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 501
#define N_THREADS 20000  //max = 20

char * path = "/f1";

typedef struct{
    int fd;
    int res;
}Mystruct;

void* fn(void* args){
    Mystruct *s = (Mystruct*) args;
    s->res = tfs_close(s->fd);
    printf("%d\n",s->res);
    return NULL;
}

int main(){
    assert(tfs_init()!=-1);
    pthread_t tid[N_THREADS];
    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);
    Mystruct s[N_THREADS];
    for(int ix = 0; ix < N_THREADS; ix++ ){
        s[ix].fd = fd;
    }
    for(int ix = 0; ix <N_THREADS; ix++){
        assert(pthread_create(&tid[ix],NULL,fn,(void*)(&s[ix])) == 0);
    }
    
    for(int ix = 0; ix< N_THREADS; ix++)
        assert(pthread_join(tid[ix],NULL) == 0);

    int i = 0;
    for (int ix = 0; ix< N_THREADS;ix++){
        if (s[ix].res == 0 && i == 0){
            i = 1;
        }
        else if (s[ix].res == 0 && i == 1){
            assert(-1==0);
        }
        else if ( s[ix].res != -1)
            assert(-1==0);
    }
    assert(i==1);

    printf("Sucessful test\n");
    exit(EXIT_SUCCESS);

}