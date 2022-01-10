#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#define DIRECT 0
#define INDIRECT 1

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a > b ? b : a)
/* Macro used when varibles with `size_t` are used, since they can't be negative */
#define sub_or_zero(nr, toSub) (toSub > nr ? 0 : nr - toSub)



int tfs_init() {
    state_init();
    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}


int tfs_destroy() {
    state_destroy();
    return 0;
}


static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}


int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}


int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }
    inode_t* inode = NULL;
    inum = tfs_lookup(name);

    /* The file already exists in the FS: */
    if (inum >= 0) {

        inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /*Since the inode exists, locks it */
        pthread_mutex_lock(&(inode->mutex));

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                /* Gets the amount of blocks the inode has */
                int n_blocks = (int) ceil((double) inode->i_size / BLOCK_SIZE), i;

                /*Frees every direct block*/
                for (i = 0; i < n_blocks && i < 10; i++)
                    if (data_block_free(inode->i_data_block[i]) == -1) {
                        pthread_mutex_unlock(&inode->mutex);
                        return -1;
                    }

                /*If there are indirect blocks, frees them and the block of blocks */
                if (n_blocks > 10) {
                    n_blocks -= 10;
                    int *block_of_blocks = (int *) data_block_get(inode->i_data_block[10]);

                    while (n_blocks != 0) {
                        if (data_block_free(*block_of_blocks) == -1) {
                            pthread_mutex_unlock(&inode->mutex);
                            return -1;
                        }

                        block_of_blocks++;
                        n_blocks--;
                    }
                    /*Block of blocks being fred */
                    if (data_block_free( inode->i_data_block[10]) == -1) {
                        pthread_mutex_unlock(&inode->mutex);
                        return -1;
                    }
                }
                /* Since there were no errors, sets the size of the inode to 0 */
                inode->i_size = 0;
            }
        }

        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        }
        else {
            offset = 0;
        }
    }

    /* The file doesn't exist in the FS: */
    else if (flags & TFS_O_CREAT) {
            /* the flags specify that it should be created */
            /* Create inode */
            inum = inode_create(T_FILE);
            if (inum == -1) {
                return -1;
            }
            /* Add entry in the root directory */
            if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
                inode_delete(inum);
                return -1;
            }
            offset = 0;
        }
    else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    int fh = add_to_open_file_table(inum,offset);
    if (inode != NULL)
        pthread_mutex_unlock(&inode->mutex);
    return fh;

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) {
    return remove_from_open_file_table(fhandle);
}


int block_create_indirect(inode_t *inode, size_t mem) {

    /* Gets the amount of blocks the inode has */
    int n_blocks = (int) ceil((double)inode-> i_size/ BLOCK_SIZE);
    /* and gets the amount of indirect blocks the inode has */
    int n_blocks_ind = max(n_blocks - 10, 0);

    /* if 0, it means the block of blocks wasn't created yet, so creates it */
    if (n_blocks_ind == 0)
        if((inode->i_data_block[10] = data_block_alloc()) == -1)
            return -1;

    /* Gets the block of blocks */
    int * block = (int*) data_block_get(inode ->i_data_block[10]);
    if (block == NULL)
        return -1;

    /* Calculates the maximum number of blocks that can be stored in the block of blocks */
    int max_possible_blocks = (int) floor((double)BLOCK_SIZE/sizeof(int));

    /* Allocates blocks until the goal size, `mem`, was reached */
    while (mem > 0 && n_blocks_ind < max_possible_blocks){

        if ((*(block + n_blocks_ind) = data_block_alloc()) == -1)
            return -1;

        n_blocks_ind++;
        mem = sub_or_zero(mem, BLOCK_SIZE);
    }

    /* If mem isn't zero, it means it tried to write more blocks than the FS allows */
    if(mem != 0)
        return -1;

    return 0;
}


int block_create(inode_t * inode, size_t mem){

    /* Gets the amount of blocks the inode has */
    int n_blocks = (int) ceil((double)inode-> i_size/ BLOCK_SIZE);

    /* If they are more than 10, only indirect blocks should be created */
    if (n_blocks >= 10)
        return block_create_indirect(inode, mem);

    /* Else, creates direct blocks until the limit */
    while (n_blocks != 10 && mem > 0){
        if((inode -> i_data_block[n_blocks] = data_block_alloc()) == -1)
            return -1;

        mem = sub_or_zero(mem, BLOCK_SIZE);
        n_blocks++;
    }
    /* If `mem` is zero, then direct blocks were enough */
    if (mem == 0)
        return 0;
    /* Else, besides the direct blocks created, more indirect blocks
     * have to be created too (with the remaining memory) */
    else if (block_create_indirect(inode, mem) == -1)
        return -1;

    return 0;
}


/* Gets next block, depending on the type (either DIRECT or INDIRECT) */
/* It is either the block nr on the nth index of the inode data_block array, or the block nr stored
 * on the block of blocks, on the right space */
void * get_next_block(int const * blocks_of_blocks, inode_t * inode, int n_blocks, int type){
    if (type == DIRECT)
        return data_block_get(inode -> i_data_block[n_blocks-1]);
    else
        return data_block_get(*(blocks_of_blocks + (n_blocks-1)));

}


ssize_t block_write(inode_t *inode, size_t * block_offset, char const *buffer, int *n_blocks, size_t *to_write, size_t to_write_cpy, size_t *mem_available, size_t *buffer_offset, int type){

    /* Gets the block of blocks and the next block to write (since they are guaranteed to be allocated already) */
    int * block_of_blocks = (int*) data_block_get(inode -> i_data_block[10]), value;
    void *block = get_next_block(block_of_blocks, inode, *n_blocks, type);

    if (block == NULL)
        return -1;

    /* Writes the first bytes on the already used block that has some space */
    size_t what_to_write = min(*mem_available, *to_write);
    memcpy(block + *block_offset, buffer + *buffer_offset, what_to_write);

    /* If everything was written, returns */
    if (what_to_write == *to_write)
        return (ssize_t) to_write_cpy;

    /* Else, there is more to write. Updates everything accordingly. */
    *buffer_offset += what_to_write;
    *to_write -= what_to_write;
    (*n_blocks)++;
    *block_offset = 0;
    *mem_available = 0;

    /* Case where the block written was the transaction block, the type becomes indirect and we only
     * consider the indirect blocks */
    if (*n_blocks == 11 && type == DIRECT){
        *n_blocks -= 10;
        type = INDIRECT;
    }

    /* Calculates the maximum number of blocks, depending on the type of writing */
    if (type == DIRECT)
        value = 10;
    else if (type == INDIRECT)
        value = (int)floor((double)BLOCK_SIZE / sizeof(int));

    /* Writes on blocks until there is nothing more to write or the max. number of blocks is exceeded */
    while (*n_blocks <= value && *to_write > 0) {
        /* Writes in blocks of BLOCK_SIZE */
        what_to_write = min(*to_write, BLOCK_SIZE);
        block = get_next_block(block_of_blocks, inode,*n_blocks,type);

        if (block == NULL)
            return -1;

        /* Copies the data */
        memcpy(block + *block_offset,  buffer + *buffer_offset, what_to_write);

        /* If the amount written was the same as what was missing to write, a.k.a., what was missing
         * to write was less than the block size, then it finished writing everything */
        if (what_to_write == *to_write)
            return (ssize_t) to_write_cpy;

        /* Else, updates the variablesa and keeps writing */
        *buffer_offset += what_to_write;
        (*n_blocks)++;
        *to_write = sub_or_zero(*to_write, BLOCK_SIZE);
    }

    return 0;
}


ssize_t data_block_write(inode_t * inode, size_t offset, char const * buffer, size_t to_write){

    ssize_t res;
    /* Gets the amount of blocks the inode has */
    int n_blocks = (int) ceil((double)offset/BLOCK_SIZE);
    /* Saves a copy of the original write size and calculates the last block's offset */
    size_t to_write_cpy = to_write, block_offset = offset % BLOCK_SIZE, buffer_offset = 0;

    /* due to the flags given, the offset only can be zero if the file is empty or the file size.
     * Therefore, if the block_offset is 0, then it doesn't have any remaining space, so we must write
     * on the next block */
    if (block_offset == 0){
        n_blocks++;
    }

    /* Gets the memory available to write on the last block */
    size_t mem_available = BLOCK_SIZE - block_offset;

    /* Writes indirectly first */
    if (n_blocks <= 10) {

        res = block_write(inode, &block_offset, buffer, &n_blocks,
                          &to_write, to_write_cpy, &mem_available, &buffer_offset, DIRECT);

        if (res == -1)
            return -1;

        /* If the result of writing was the original write size, then everything was written, so returns */
        else if(res == to_write_cpy)
            return (ssize_t) to_write_cpy;

        /* Else, we must continue writing in indirect blocks */
        /* Resets the memory available because we are going to a brand new block */
        mem_available = BLOCK_SIZE;
    }

    /* Gets the number of indirect blocks and writes in INDIRECT type */
    n_blocks -= 10;
    res = block_write(inode, &block_offset, buffer, &n_blocks,
                      &to_write, to_write_cpy, &mem_available,&buffer_offset,INDIRECT);

    if (res == -1)
        return -1;

    /* Verifies if everything was written. If not, there was not enough space */
    else if (res == to_write_cpy)
        return (ssize_t) to_write_cpy;

    return -1;
}


ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {

    /* Gets the file from the file handle to write on */
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    /* The file is locked because no other threads should interact with it until it has finished
     * being written */
    pthread_mutex_lock(&file->mutex);
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_mutex_unlock(&file ->mutex);
        return -1;
    }

    /* locks the inode, because everything on it will be updated, and gets the available memory */
    pthread_mutex_lock(&inode->mutex);
    int n_blocks = (int) ceil((double)inode -> i_size/ BLOCK_SIZE);
    size_t mem_available = (size_t) (n_blocks*BLOCK_SIZE) - inode->i_size;
    if (to_write > mem_available){
        /* If there is less memory available than what is expected to be written, creates enough blocks */
        if (block_create(inode, to_write - mem_available) == -1) {
            pthread_mutex_unlock(&inode->mutex);
            pthread_mutex_unlock(&file->mutex);
            return -1;
        }
    }

    if (to_write > 0) {
        /* Writes the data, now with enough space */
        data_block_write(inode, file->of_offset, buffer, to_write);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    /* Now that everything was written, unlocks both the inode and the file */
    pthread_mutex_unlock(&inode->mutex);
    pthread_mutex_unlock(&file-> mutex);

    return (ssize_t) to_write;
}


ssize_t data_block_read(void *buffer, inode_t *inode, size_t offset, size_t to_read){

    /* Gets the number of blocks from where to start to read and other calculations */
    /* If the offset is zero, then forces the number of blocks to 1 ( 0/any_number = 0) */
    int n_blocks = (offset == 0) ? 1 :(int) ceil((double)offset/BLOCK_SIZE);
    size_t block_offset = offset % BLOCK_SIZE;
    size_t buffer_offset = 0,to_read_cpy = to_read;

    /* Case where there are only direct blocks */
    if (n_blocks <= 10) {

        /* If block offset isn't zero, we want to read just a part of the starting block */
        if (block_offset != 0) {
            void *block = data_block_get(inode->i_data_block[n_blocks - 1]);

            if (block == NULL)
                return -1;

            /* The data is read and the block_offset is reseted */
            size_t what_to_read = min(to_read, BLOCK_SIZE - block_offset);
            memcpy(buffer + buffer_offset, block + block_offset, what_to_read);
            buffer_offset += what_to_read;

            /* If they match, it means everything was read, so returns */
            if(what_to_read == to_read)
                return (ssize_t) to_read_cpy;

            /* Updates the variables accordingly*/
            n_blocks++;
            to_read = sub_or_zero(to_read, what_to_read);
            block_offset = 0;
        }

        /* Keeps reading until the end of the direct blocks */
        while (n_blocks <= 10 && to_read > 0){
            void * block = data_block_get(inode-> i_data_block[n_blocks-1]);
            if (block == NULL)
                return -1;

            size_t what_to_read = min(to_read, BLOCK_SIZE);
            memcpy(buffer + buffer_offset, block, what_to_read);

            if(what_to_read == to_read)
                return (ssize_t) to_read_cpy;

            buffer_offset += what_to_read;
            n_blocks++;
            to_read = sub_or_zero(to_read, what_to_read);
        }

    }

    /* If n_blocks was greater than 10 or it didn't return in the above condition, it means there
     * is more to read, now in the indirect blocks */
    /* So, gets the block of blocks and the right starting block */
    n_blocks -= 10;
    int* block_of_blocks = (int*)data_block_get(inode->i_data_block[10]);

    if (block_of_blocks == NULL)
        return -1;

    void *block = data_block_get(*(block_of_blocks + n_blocks-1));

    if (block == NULL)
        return -1;

    /* If block_offset isn't zero, we want to skip a part of the starting block (doesn't happen if
     * coming from direct blocks, because the block_offset is set to 0) */
    if (block_offset != 0) {

        size_t what_to_read = min(to_read, BLOCK_SIZE - block_offset);
        memcpy(buffer + buffer_offset, block + block_offset, what_to_read); //FIXME VER SE O QUE E PARA LER E MENOR QUE ESTE VALOR

        if(what_to_read == to_read)
            return (ssize_t) to_read_cpy;

        buffer_offset += what_to_read;
        n_blocks++;
        to_read = sub_or_zero(to_read, what_to_read);
    }

    /* Reads to the buffer while there are bytes to be read */
    while (to_read > 0){
        void * block1 = data_block_get(*(block_of_blocks + n_blocks-1));

        size_t what_to_read = min(to_read, BLOCK_SIZE);
        memcpy(buffer + buffer_offset,block1,what_to_read);

        if(what_to_read == to_read)
            return (ssize_t) to_read_cpy;

        buffer_offset += what_to_read;
        n_blocks++;
        to_read = sub_or_zero(to_read, what_to_read);
    }

    return -1;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    /* Gets the file from the file handle */
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* Locks the file until everything is read and from the open file table entry, we get the inode */
    pthread_mutex_lock(&(file->mutex));
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_mutex_unlock(&file->mutex);
        return -1;
    }

    /* Since the inode exists, we lock it and determine how many bytes to read */
    pthread_mutex_lock(&(inode->mutex));
    size_t to_read = inode->i_size - file->of_offset; //
    if (to_read > len) {
        to_read = len;
    }

    /* Perform the actual read */
    size_t offset = file->of_offset;
    data_block_read(buffer, inode, offset, to_read);

    /* Everything is unlocked and the offset associated with the file handle is
     * incremented accordingly */
    pthread_mutex_unlock(&inode->mutex);
    file->of_offset += to_read;
    pthread_mutex_unlock(&(file->mutex));


    return (ssize_t)to_read;
}


int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {

    /* Checks if the path name is valid */
    if (!valid_pathname(source_path)) {
        return -1;
    }

    /* Given the path, gets the inumber */
    int inum = tfs_lookup(source_path);
    if (inum == -1)
        return -1;

    /* Given the inumber, gets the inode */
    inode_t *inode = inode_get(inum);
    if (inode == NULL)
        return -1;

    /* Since everything is in order, gets the appropriate variables and adds the file to the
     * open file table */
    size_t size = inode->i_size;
    char *buffer = malloc(size + 1);
    int fh = add_to_open_file_table(inum, 0);

    /* Uses tfs_read to read to the buffer */
    tfs_read(fh, buffer, size);
    tfs_close(fh);

    /* Writes externally */
    FILE *f = fopen(dest_path, "w+");

    if (f == NULL)
        return -1;

    size_t bytes_written = fwrite(buffer, sizeof(char), size, f);

    if (bytes_written != size)
        return -1;

    fclose(f);
    free(buffer);

    return 0;
}

