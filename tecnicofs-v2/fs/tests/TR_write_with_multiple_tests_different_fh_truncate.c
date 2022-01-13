#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

char * path = "/f1";

void* fn1(void * arg){
    char * buffer = (char*) arg;
    int fd = tfs_open(path,TFS_O_TRUNC);
    sleep(0.5);
    assert(fd !=-1);
    assert(tfs_write(fd,buffer,18) == 18);
    assert(tfs_close(fd)!=-1);
    return NULL;
}


void* fn2(void * arg){
    char * buffer = (char*) arg;
    int fd = tfs_open(path,TFS_O_TRUNC);
    sleep(0.2);
    assert(fd !=-1);
    assert(tfs_write(fd,buffer,21) == 21);
    assert(tfs_close(fd)!=-1);
    return NULL;
}

void* fn3(void * arg){
    char * buffer = (char*) arg;
    int fd = tfs_open(path,TFS_O_TRUNC);
    sleep(0.7);
    assert(fd !=-1);
    assert(tfs_write(fd,buffer,6) == 6);
    assert(tfs_close(fd)!=-1);
    return NULL;
}


void* fn4(void * arg){
    char * buffer = (char*) arg;
    int fd = tfs_open(path,TFS_O_TRUNC);
    assert(fd !=-1);
    assert(tfs_write(fd,buffer,56) == 56);
    assert(tfs_close(fd)!=-1);
    return NULL;
}



int main(){
    pthread_t t1;
    pthread_t t2;
    pthread_t t3;
    pthread_t t4;
    assert(tfs_init() == 0);
    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd!=-1);
    assert(tfs_close(fd)!=-1);
    char* buffer1 = "oiwendwoendweoi wd";
    char*buffer2 = "pomwdpomwecpowmwcweop";
    char *buffer3 = "inxwoi"; 
    char*buffer4 = "powpocmweopc   dwepoddmwe we dwe odmwed wd emd weopd mwe";

    pthread_create(&t1,NULL,fn1,(void*)buffer1);
    pthread_create(&t2,NULL,fn2,(void*)buffer2);  
    pthread_create(&t3,NULL,fn3,(void*)buffer3);
    pthread_create(&t4,NULL,fn4,(void*)buffer4);

    pthread_join(t1,NULL);
    pthread_join(t2,NULL);
    pthread_join(t3,NULL);
    pthread_join(t4,NULL);
    char output[100];
    fd = tfs_open(path,0);
    assert(fd !=-1);
    ssize_t read = tfs_read(fd,output,100);
    output[read] = '\0';
    int res1 = strcmp(output,buffer1);
    int res2 = strcmp(output,buffer2);
    int res3 = strcmp(output,buffer3);
    int res4 = strcmp(output,buffer4);

    assert(!res1 || !res2 || !res3 || !res4);
    assert(tfs_close(fd)!=-1);
    printf("SUCCESS\n");
    exit(EXIT_SUCCESS);
}


