#include "operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE_TO_TEST (1024*20)


int main() {

    char big_str[SIZE_TO_TEST+1];

    memset(big_str, 'b', sizeof(big_str));
    big_str[SIZE_TO_TEST] = '\0';

    char buffer[SIZE_TO_TEST];

    char *path = "/f1";
    char *path2 = "./test10.txt";

    printf("Size to test = %ld\n", sizeof(buffer));

    memset(buffer, '\0', sizeof(buffer));

    memcpy(buffer, big_str, SIZE_TO_TEST);

    assert(tfs_init() != -1);

    int tfs_file = tfs_open(path, TFS_O_CREAT);
    assert(tfs_file != -1);

    assert(tfs_write(tfs_file, buffer, strlen(buffer)) == strlen(buffer));

    assert(tfs_close(tfs_file) != -1);

    assert(tfs_copy_to_external_fs(path, path2) != -1);

    // read to copied file - to to_read - and compare it to to_write

    FILE *fp = fopen(path2, "r");

    assert(fp != NULL);

    assert(fread(buffer, sizeof(char), strlen(big_str), fp) == strlen(big_str));

    assert(strncmp(big_str, buffer, strlen(big_str)) == 0);
    
    assert(fclose(fp) != -1);

    unlink(path2);

    printf("======> Successful test.\n\n");

    return 0;
}
