#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define COUNT 80
#define SIZE 27
#define N_THREADS 10099//max = 10476

typedef struct{
    ssize_t res;
    char* buffer;
} Mystruct;

int fd;
void* fn(void* arg){
    Mystruct s = *((Mystruct *)arg);
    s.res = tfs_write(fd,s.buffer, SIZE);
    if(s.res != SIZE)
	    printf("%ld\n",s.res);
    assert(s.res <= SIZE);
    return NULL;
}


int main() {
    pthread_t threads[N_THREADS];
    char *path = "/f1";

    /* Writing this buffer multiple times to a file stored on 1KB blocks will 
       always hit a single block (since 1KB is a multiple of SIZE=256) */
    char *write = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    assert(tfs_init() != -1);

    fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);
    Mystruct s[N_THREADS];
    for(int ix = 0; ix< N_THREADS; ix++)
        s[ix].buffer = write;
    for (int i = 0; i < N_THREADS; i++) {
            int suc = pthread_create(&threads[i], NULL, fn, (void *) &(s[i]));
            assert(suc == 0);
        }

    for (int i = 0; i < N_THREADS; i++){
        pthread_join(threads[i], NULL);
    }
    assert(tfs_close(fd)!=-1);
    int i = 0;
    int counter = 0;
    for(int ix = 0; ix<N_THREADS;ix++){
        if(s[ix].res == 8)
            i++;

        else if(s[ix].res == 0)
           counter++;
          
	else if (s[ix].res != SIZE && s[ix].res != 0 && s[ix].res != 8)
		assert(0);
    }
    assert(i == 1 && counter == 10);
    printf("Sucessful test\n");
    return 0;
}                                                                                                                                                                                                                                                                                                                   
