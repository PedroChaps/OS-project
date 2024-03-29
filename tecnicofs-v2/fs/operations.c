#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#define DIRECT 0
#define INDIRECT 1
#define ALREADY_OPENED 1
#define NOT_OPENED_YET 0 

#define max(a, b) (a > b ? a : b)
#define min(a, b) (a > b ? b : a)
/* Macro used when varibles with `size_t` are used, since they can't be negative */
#define sub_or_zero(nr, toSub) (toSub > nr ? 0 : nr - toSub)



int tfs_init() {
    state_init();
    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return ERROR;
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
        return ERROR;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}


int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset = 0;
    int open_type;
    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return ERROR;
    }
    inode_t* inode = NULL;
    inum = tfs_lookup(name);

    /* The file already exists in the FS: */
    if (inum >= 0) {

        inode = inode_get(inum);
        if (inode == NULL) {
            return ERROR;
        }

        /*Since the inode exists, locks it */
        pthread_rwlock_wrlock(&(inode->rwlock));

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                /* Gets the amount of blocks the inode has */
                int n_blocks = (int) ceil((double) inode->i_size / BLOCK_SIZE), i;

                /*Frees every direct block*/
                for (i = 0; i < n_blocks && i < 10; i++)
                    if (data_block_free(inode->i_data_block[i]) == ERROR) {
                        pthread_rwlock_unlock(&inode->rwlock);
                        return ERROR;
                    }

                /*If there are indirect blocks, frees them and the block of blocks */
                if (n_blocks > 10) {
                    n_blocks -= 10;
                    int *block_of_blocks = (int *) data_block_get(inode->i_data_block[10]);

                    while (n_blocks != 0) {
                        if (data_block_free(*block_of_blocks) == ERROR) {
                            pthread_rwlock_unlock(&inode->rwlock);
                            return ERROR;
                        }

                        block_of_blocks++;
                        n_blocks--;
                    }
                    /*Block of blocks being fred */
                    if (data_block_free( inode->i_data_block[10]) == ERROR) {
                        pthread_rwlock_unlock(&inode->rwlock);
                        return ERROR;
                    }
                }
                /* Since there were no errors, sets the size of the inode to 0 */
                inode->i_size = 0;
            }
        }

        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            open_type = TFS_O_APPEND;
        }
        else {
            open_type = TFS_O_TRUNC;
        }
    }

    /* The file doesn't exist in the FS: */
    else if (flags & TFS_O_CREAT) {
            /* the flags specify that it should be created */
            /* Create inode */
            inum = inode_create(T_FILE);
            if (inum == ERROR) {
                return ERROR;
            }
            /* Add entry in the root directory */
            if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == ERROR) {
                inode_delete(inum);
                return ERROR;
            }
            open_type = TFS_O_CREAT;
        }
    else {
        return ERROR;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    int fh = add_to_open_file_table(inum,offset);
    open_file_entry_t *file  = get_open_file_entry(fh);
    pthread_mutex_lock(&file->mutex);
    file->open_type = open_type;
    file->has_opened = NOT_OPENED_YET;
    pthread_mutex_unlock(&file->mutex);
    if (inode != NULL)
        pthread_rwlock_unlock(&inode->rwlock);
    return fh;

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) {
    return remove_from_open_file_table(fhandle);
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
        return ERROR;

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
            return ERROR;

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

        if (res == ERROR)
            return ERROR;

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

    if (res == ERROR)
        return ERROR;

    /* Verifies if everything was written. If not, there was not enough space */
    else if (res == to_write_cpy)
        return (ssize_t) to_write_cpy;

    return ERROR;
}


/* Function that checks if blocks are already allocated and if not, allocates them */
int create_necessary_blocks(size_t offset, inode_t *inode, size_t *to_write){
    
    /* Checks if the amount to write exceeds the amount possible. 
       If it does, then only allows to write the maximum possible. */
    if(*to_write + offset > INODE_SIZE_AVAILABLE)
        *to_write = INODE_SIZE_AVAILABLE - offset;
    
    /* Gets the start block, end block and the offset on the starting block */
    int starting_block = max(1, (int) ceil((double) offset / BLOCK_SIZE));
    int end_block = min((int)ceil((double) (offset + *to_write) / BLOCK_SIZE),INODE_DIRECT_ENTRIES + INODE_INDIRECT_ENTRIES*BLOCKS_IN_A_INDIRECT_BLOCK);
    //size_t block_offset = offset % BLOCK_SIZE;
    
    /* Iterates between the starting block and the end block, allocating the blocks that haven't been allocated yet regarding direct entries */
    while(starting_block <= INODE_DIRECT_ENTRIES && starting_block <= end_block){
        
        if(inode->i_data_block[starting_block-1] == NOT_ALLOCD)
            if ((inode-> i_data_block[starting_block-1] = data_block_alloc()) == ERROR)
                return ERROR;
        starting_block++;
    }
    
    /* If the cycle ended because starting_block == end_block, then all the necessary
       blocks have been accounted for, so returns. */
    if (starting_block == end_block+1){
        return 0; 
    }
    
    /* If not, then indirect blocks must be checked first: */
    
    int *block_of_blocks = NULL;
    /* Updates the starting_block and end_block so they are easier to use */
    starting_block -= INODE_DIRECT_ENTRIES;
    end_block -= INODE_DIRECT_ENTRIES;
    
    /* Checks if block_of_blocks was created already and, if not, creates it and 
       resets it's entries */
    if (inode->i_data_block[INODE_DIRECT_ENTRIES] == NOT_ALLOCD){
        /* Allocates block and gets it */
        if((inode->i_data_block[INODE_DIRECT_ENTRIES] = data_block_alloc()) == ERROR)
            return ERROR;
        block_of_blocks = (int *) data_block_get(inode->i_data_block[INODE_DIRECT_ENTRIES]);
        if (block_of_blocks == NULL)
            return ERROR;
        /* resets it's entries */
        for(int i = 0; i < BLOCKS_IN_A_INDIRECT_BLOCK; i++)
            block_of_blocks[i] = NOT_ALLOCD;
    }
    
    /* Sets the block of blocks if it hadn't been set yet */
    block_of_blocks = (int *) data_block_get(inode->i_data_block[INODE_DIRECT_ENTRIES]);
    
    /* Now, checks indirect entries */
    while (starting_block <= end_block) {
    
        if (block_of_blocks[starting_block-1] == NOT_ALLOCD)
            if ((block_of_blocks[starting_block-1] = data_block_alloc()) == ERROR)
                return ERROR;
        starting_block++;
    }
    
    return 0;
}



ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {

    /* Gets the file from the file handle to write on */
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return ERROR;
    }

    /* From the open file table entry, we get the inode */
    /* The file is locked because no other threads should interact with it until 
       it has finished being written */
    pthread_mutex_lock(&file->mutex);
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_mutex_unlock(&file ->mutex);
        return ERROR;
    }

    /* locks the inode, because everything on it will be updated, and gets 
       the available memory */
    /* the available memory (to write) is all the memory possible in a single file 
       minus the open file offeset */
    pthread_rwlock_wrlock(&inode->rwlock);
    if(file->has_opened == NOT_OPENED_YET){
        if (file->open_type == TFS_O_APPEND){
        file-> of_offset = inode -> i_size;
        }
        file->has_opened = ALREADY_OPENED;
    }
           
    /* Checks if all necessary blocks are already allocated 
       and if not, creates them */     
    if (create_necessary_blocks(file->of_offset, inode, &to_write) == ERROR){
        pthread_rwlock_unlock(&inode->rwlock);
        pthread_mutex_unlock(&file->mutex);
        return ERROR;
    }

    /* Writes the data, now with enough space */
    data_block_write(inode, file->of_offset, buffer, to_write);

    /* The offset associated with the file handle is 
        incremented accordingly */
    file->of_offset += to_write;
    if (file->of_offset > inode->i_size) {
        inode->i_size = file->of_offset;
    }

    /* Now that everything was written, unlocks both the inode and the file */
    pthread_rwlock_unlock(&inode->rwlock);
    pthread_mutex_unlock(&file-> mutex);

    return (ssize_t) to_write;
}


ssize_t block_read(char * buffer, int const *block_of_blocks,int *n_blocks, size_t *block_offset, size_t * buffer_offset, inode_t * inode,size_t * to_read,size_t to_read_cpy, int type){

    /* Gets the next block */
    void * block = get_next_block(block_of_blocks,inode,*n_blocks,type);
    if(block == NULL)
        return ERROR;

    /* Calculates the amount to read and reads it to the buffer */
    size_t what_to_read = min(*to_read, BLOCK_SIZE - *block_offset);
    memcpy(buffer + *buffer_offset,block + *block_offset,what_to_read);

    if(what_to_read == *to_read)
        return (ssize_t) to_read_cpy;

    /* Updates the variables */
    *buffer_offset += what_to_read;
    (*n_blocks)++;
    *to_read = sub_or_zero(*to_read, what_to_read);
    return 0;
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
            ssize_t res = block_read(buffer,NULL,&n_blocks,&block_offset,&buffer_offset,inode,&to_read, to_read_cpy, DIRECT);
            if (res == ERROR)
                return ERROR;
            else if (res == (ssize_t)to_read_cpy)
                return res;
            block_offset = 0;
        }

        /* Keeps reading until the end of the direct blocks */
        while (n_blocks <= 10 && to_read > 0){
            ssize_t res = block_read(buffer,NULL,&n_blocks,&block_offset,&buffer_offset,inode,&to_read, to_read_cpy, DIRECT);
            if (res == ERROR)
                return ERROR;
            else if(res == (ssize_t) to_read_cpy)
                return res;    
        }

    }

    /* If n_blocks was greater than 10 or it didn't return in the above condition, it means there
     * is more to read, now in the indirect blocks */
    /* So, gets the block of blocks and the right starting block */
    n_blocks -= 10;
    int* block_of_blocks = (int*)data_block_get(inode->i_data_block[10]);

    if (block_of_blocks == NULL)
        return ERROR;

    /* If block_offset isn't zero, we want to skip a part of the starting block (doesn't happen if
     * coming from direct blocks, because the block_offset is set to 0) */
    if (block_offset != 0) {

        ssize_t res = block_read(buffer,block_of_blocks,&n_blocks,&block_offset,&buffer_offset,inode,&to_read, to_read_cpy, INDIRECT);
        if (res == ERROR)
            return ERROR;
        else if(res == (ssize_t) to_read_cpy)
            return res;
        block_offset = 0;
    }

    /* Reads to the buffer while there are bytes to be read */
    while (to_read > 0){
        ssize_t res = block_read(buffer,block_of_blocks,&n_blocks,&block_offset,&buffer_offset,inode,&to_read, to_read_cpy, INDIRECT);
        if (res == ERROR)
            return ERROR;
        else if(res == (ssize_t) to_read_cpy)
            return res;
    }

    return ERROR;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    /* Gets the file from the file handle */
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return ERROR;
    }

    /* Locks the file until everything is read and from the open file table entry, we get the inode */
    pthread_mutex_lock(&(file->mutex));
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_mutex_unlock(&file->mutex);
        return ERROR;
    }

    /* Since the inode exists, we lock it and determine how many bytes to read */
    pthread_rwlock_rdlock(&(inode->rwlock));
    if(file->has_opened == NOT_OPENED_YET){
        if (file->open_type == TFS_O_APPEND){
        file-> of_offset = inode -> i_size;
        }
        file->has_opened = ALREADY_OPENED;
    }

    /* Gets the amount to read and checks if it is above the limit */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    /* Perform the actual read */
    size_t offset = file->of_offset;
    data_block_read(buffer, inode, offset, to_read);

    /* Everything is unlocked and the offset associated with the file handle is
     * incremented accordingly */
    pthread_rwlock_unlock(&inode->rwlock);
    file->of_offset += to_read;
    pthread_mutex_unlock(&(file->mutex));


    return (ssize_t)to_read;
}


int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {

    /* Checks if the path name is valid */
    if (!valid_pathname(source_path)) {
        return ERROR;
    }

    /* Given the path, gets the inumber */
    int inum = tfs_lookup(source_path);
    if (inum == ERROR)
        return ERROR;

    /* Given the inumber, gets the inode */
    inode_t *inode = inode_get(inum);
    if (inode == NULL)
        return ERROR;

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
        return ERROR;

    size_t bytes_written = fwrite(buffer, sizeof(char), size, f);

    if (bytes_written != size)
        return ERROR;

    fclose(f);
    free(buffer);

    return 0;
}