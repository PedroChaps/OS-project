#include "operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE_TO_TEST (1024 * 71 + 5)


int main() { // Passou! :D

    char big_str[SIZE_TO_TEST];

    memset(big_str, 'b', sizeof(big_str));

    char buffer[SIZE_TO_TEST];

    char *path = "/f1";
    char *path2 = "/home/andre/Desktop/SO/project/SO-projeto/tecnicofs-v2/fs/tests/escreveaquibro.txt";

    printf("Size to test = %ld\n", sizeof(buffer));

    memset(buffer, '\0', sizeof(buffer));

    memcpy(buffer, big_str, SIZE_TO_TEST);

    assert(tfs_init() != -1);

    int tfs_file = tfs_open(path, TFS_O_CREAT);
    assert(tfs_file != -1);

    assert(tfs_write(tfs_file, buffer, sizeof(buffer)) == sizeof(buffer));

    assert(tfs_close(tfs_file) != -1);

    assert(tfs_copy_to_external_fs(path, path2) != -1);

    // read to copied file - to to_read - and compare it to to_write

    FILE *fp = fopen(path2, "r");

    assert(fp != NULL);

    assert(fread(buffer, sizeof(char), sizeof(buffer), fp) == sizeof(buffer));
    
    assert(fclose(fp) != -1);

    unlink(path2);

    printf("======> Successful test.\n\n");

    return 0;
}

// Falta testar escrever um ficheiro a seguir ao outro
