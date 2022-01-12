#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define COUNT 80
#define SIZE 501
#define N_THREADS 20  //max = 20

char * path = "/f1";

void* fn(void* args){
    char * buffer = (char*) args; 
    int fd = tfs_open(path,0);
    assert(fd !=-1);
    assert(tfs_read(fd,buffer,SIZE)== SIZE);
    assert(tfs_close(fd)!=-1);
    return NULL;
}

int main(){
    assert(tfs_init()!=-1);
    char **outputs = (char**) malloc(sizeof(char *) * N_THREADS);
    for (int ix = 0; ix< N_THREADS; ix++)
        outputs[ix] = (char*) malloc(sizeof(char)* SIZE);
    pthread_t tid[N_THREADS];
    char * write = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ  foinbed9owehnddnweod n woeind weoidn  weoidn weid nwed ndo 2ndi23nd 2o3ind 23oid n23od n23iod n23dio n23ed oin23di n23oid n23oind 23oidn 23iond 23iond 23iond23oind 23ind 32oind 23ind 23oid n23ind 23oi nd23ion d2io3nd 23ione 23ion ";
    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);
    assert(tfs_write(fd,write,SIZE) == SIZE);
    assert(tfs_close(fd)!=-1);
    for(int ix = 0; ix <N_THREADS; ix++){
        assert(pthread_create(&tid[ix],NULL,fn,(void*)outputs[ix]) == 0);
    }
    
    for(int ix = 0; ix< N_THREADS; ix++)
        assert(pthread_join(tid[ix],NULL) == 0);


    for (int ix = 0; ix< N_THREADS; ix++){
        assert(!memcmp(write,outputs[ix],SIZE));
        free(outputs[ix]);
        }

    free(outputs);
    printf("Sucesful test\n");
    exit(EXIT_SUCCESS);

}
