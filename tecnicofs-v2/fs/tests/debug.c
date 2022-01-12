#include "operations.h"
#include <assert.h>
#include <string.h>

#define MAX_SIZE INODE_SIZE_AVAILABLE
#define EXTRA 46
#define SIZE 27 

int main() {

    assert(tfs_init() != -1);

    char bufferIn[MAX_SIZE + EXTRA];
    char bufferOut[MAX_SIZE];
    char* write = "IOVNERVINVEORIVNE P EW  WE ";
    int of = 0;
    int a  = 10090;
    while(a != 0){
        memcpy(bufferIn,write,SIZE);
        of += SIZE;
        a--;
    }


    int fileIn = tfs_open("/testfile", TFS_O_CREAT);
    assert(fileIn != -1);    

    ssize_t r = tfs_write(fileIn, bufferIn, MAX_SIZE);
    printf("%ld\n",r);
    assert(r == MAX_SIZE);

    int fileOut = tfs_open("/testfile", 0);
    assert(fileOut != -1);
    of = 0;
    for (int ix = 0; ix< 10090; ix++){
        tfs_read(fileOut,bufferOut + of, SIZE);
        of+= SIZE;
    }
    
    assert(memcmp(bufferIn, bufferOut, MAX_SIZE) == 0);


    assert(tfs_close(fileIn) == 0);
    assert(tfs_close(fileOut) == 0);

    printf("Successful test.\n");

    return 0;
}
