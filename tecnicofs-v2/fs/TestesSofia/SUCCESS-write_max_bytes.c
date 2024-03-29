#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define SIZE (272384)

/**
   This test fills in a new file up to 20 blocks via multiple writes
   (therefore causing the file to hold 10 direct references + 10 indirect
   references from a reference block),
   each write always targeting only 1 block of the file, 
   then checks if the file contents are as expected
 */


int main() {

   char *path = "/f1";
   int fd = 0;

   /* Writing this buffer multiple times to a file stored on 1KB blocks will 
      always hit a single block (since 1KB is a multiple of SIZE=256) */
   char input[SIZE]; 
   memset(input, 'R', SIZE);

   char output[SIZE];
   memset(output, '\0', SIZE);

   assert(tfs_init() != -1);

   /* Write input COUNT times into a new file */
   fd = tfs_open(path, TFS_O_CREAT);

   assert(fd != -1);

   assert(tfs_write(fd, input, SIZE) == SIZE);

   printf("Test finished!\n");

   assert(tfs_close(fd) != -1);

   /* Open again to check if contents are as expected */
   fd = tfs_open(path, 0);

   assert(fd != -1);

   assert(tfs_read(fd, output, SIZE) == SIZE);

   for(int i = 0; i < SIZE; i++) {
      if (input[i] != output[i])
         printf("(%d) |%c| vs |%c|\n",i, input[i], output[i]);

      assert(input[i] == output[i]);
   }

   printf("memcmp = %d\n", memcmp(input, output, SIZE));

   assert(memcmp(input, output, SIZE) == 0);

   assert(tfs_close(fd) != -1);


   printf("======> Sucessful test\n\n");

   return 0;
}