#include "state.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#define min(a, b) (a > b ? b : a)


/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table */
static inode_t inode_table[INODE_TABLE_SIZE];
static char freeinode_ts[INODE_TABLE_SIZE];
static pthread_mutex_t freeinode_mutex;

/* Data blocks */
static char fs_data[BLOCK_SIZE * DATA_BLOCKS];
static char free_blocks[DATA_BLOCKS];
static pthread_mutex_t free_blocks_mutex;

/* Volatile FS state */
static open_file_entry_t open_file_table[MAX_OPEN_FILES];
static char free_open_file_entries[MAX_OPEN_FILES];
static pthread_mutex_t free_OF_mutex;

pthread_mutex_t directory_mutex;

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
void state_init() {

    /* Initializes the mutexes and the arrays */
    pthread_mutex_init(&freeinode_mutex,NULL);
    pthread_mutex_init(&free_OF_mutex,NULL);
    pthread_mutex_init(&free_blocks_mutex,NULL);
    pthread_mutex_init(&directory_mutex,NULL);

    for (int ix = 0; ix < BLOCK_SIZE * DATA_BLOCKS; ix++ ){ //TODO Remover este ciclo e ver os "not sure" cá em baixo
        fs_data[ix] = FREE;
    }

    for (int ix = 0; ix < INODE_TABLE_SIZE; ix++ ){
        pthread_mutex_init(&inode_table[ix].mutex,NULL);
    }

    for (int ix = 0; ix < MAX_OPEN_FILES;ix++)
        pthread_mutex_init(&open_file_table[ix].mutex,NULL);

    pthread_mutex_lock(&freeinode_mutex); //not sure
    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        freeinode_ts[i] = FREE;
    }
    pthread_mutex_unlock(&freeinode_mutex); //not sure

    pthread_mutex_lock(&free_blocks_mutex); //not sure
    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        free_blocks[i] = FREE;
    }
    pthread_mutex_unlock(&free_blocks_mutex); //not sure

    pthread_mutex_lock(&free_OF_mutex); //not sure
    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        free_open_file_entries[i] = FREE;
    }
    pthread_mutex_unlock(&free_OF_mutex); //not sure
}


void state_destroy() {

    /* Destroys the mutexes */
    pthread_mutex_destroy(&freeinode_mutex);
    pthread_mutex_destroy(&free_OF_mutex);
    pthread_mutex_destroy(&free_blocks_mutex);
    pthread_mutex_destroy(&directory_mutex);
    for (int ix = 0; ix < INODE_TABLE_SIZE; ix++ ){
        pthread_mutex_destroy(&inode_table[ix].mutex);
    }
    for (int ix = 0; ix < MAX_OPEN_FILES;ix++)
        pthread_mutex_destroy(&open_file_table[ix].mutex);
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */
int inode_create(inode_type n_type) {

    /* Locks the free_inode table so an empty slot can be found without problems */
    pthread_mutex_lock(&freeinode_mutex);
    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {

        if ((inumber * (int) sizeof(allocation_state_t) % BLOCK_SIZE) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }

        /* Finds first free entry in i-node table */
        if (freeinode_ts[inumber] == FREE) {
            /* Found a free entry, so uses it the new i-node */
            freeinode_ts[inumber] = TAKEN;
            insert_delay(); // simulate storage access delay (to i-node)
            pthread_mutex_lock(&inode_table[inumber].mutex); //FIXME METER ISTO AQUI OU...
            inode_table[inumber].i_node_type = n_type;
            //FIXME AQUI??????
            if (n_type == T_DIRECTORY) {
                /* Initializes directory (filling its block with empty
                 * entries, labeled with inumber==-1) */
                int b = data_block_alloc();
                if (b == -1) {
                    freeinode_ts[inumber] = FREE;
                    pthread_mutex_unlock(&freeinode_mutex);
                    return -1;
                }
                inode_table[inumber].i_size = BLOCK_SIZE;
                inode_table[inumber].i_data_block[0]= b;
                pthread_mutex_unlock(&inode_table[inumber].mutex);

                /* Gets the associated data block */
                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);
                if (dir_entry == NULL) {     //FIXME POR FAZER DA LINHA 131 À 139 MAS A PARTIDA NAO E PRECISO PORQUE SO HA UMA DIRETORIA QUE E A RAIZ
                    freeinode_ts[inumber] = FREE;
                    pthread_mutex_unlock(&freeinode_mutex);
                    return -1;
                }
                pthread_mutex_lock(&directory_mutex);
                /* Initializes the directory entry */
                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
                pthread_mutex_unlock(&directory_mutex);
            }
            else {
                /* In case of a new file, simply sets its size to 0 */
                inode_table[inumber].i_size = 0;
                pthread_mutex_unlock(&inode_table[inumber].mutex);
            }
            pthread_mutex_unlock(&freeinode_mutex);
            return inumber;
        }
    }
    pthread_mutex_unlock(&freeinode_mutex);
    return -1;
}


/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {

    /* Simulate storage access delay (to i-node and freeinode_ts) */
    insert_delay();
    insert_delay();

    /* Locks the free_inode table to avoid parallelization problems */
    pthread_mutex_lock(&freeinode_mutex);
    if (!valid_inumber(inumber) || freeinode_ts[inumber] == FREE) {
        pthread_mutex_unlock(&freeinode_mutex);
        return -1;
    }

    /* Sets the entry to FREE and unlocks the table */
    freeinode_ts[inumber] = FREE;
    pthread_mutex_unlock(&freeinode_mutex);

    /* Gets the inode and locks it */
    inode_t * inode = &inode_table[inumber];
    pthread_mutex_lock(&(inode->mutex));

    /* Frees blocks until there are no more blocks */
    if (inode->i_size > 0) {
        int n_blocks = (int) ceil((double)inode->i_size/ BLOCK_SIZE);
        int min = min(n_blocks, 10);

        /* Frees the direct blocks */
        for (int i = 0; i < min; i++)
            if (data_block_free(inode->i_data_block[i]) == -1)
                return -1;

        /* If there are indirect blocks, frees them */
        n_blocks -= 10;
        if (n_blocks > 0){
            int *block_of_blocks = (int*) data_block_get(inode->i_data_block[10]);

            while (n_blocks != 0) {
                if (data_block_free((block_of_blocks[n_blocks - 1])) == -1)
                    return -1;
                n_blocks--;
            }

            /* Frees the block of blocks */
            data_block_free(inode->i_data_block[10]);
        }

    }

    pthread_mutex_unlock(&(inode->mutex));
    return 0;
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node
    return &inode_table[inumber];
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

    /* Simulate storage access delay to i-node with inumber */
    insert_delay();

    pthread_mutex_lock(&inode_table[inumber].mutex); // FIXME acho que aqui temos de ter o unlock so no fim, nao pode ser logo na linha a seguir

    if (inode_table[inumber].i_node_type != T_DIRECTORY || strlen(sub_name) == 0) {
        pthread_mutex_unlock(&inode_table[inumber].mutex);
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *) data_block_get(inode_table[inumber].i_data_block[0]);
    if (dir_entry == NULL) {
        return -1;
    }

    pthread_mutex_unlock(&inode_table[inumber].mutex);


    pthread_mutex_lock(&directory_mutex);
    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {

        if (dir_entry[i].d_inumber == -1) {
            /* Once an empty entry is found, copies the inumber and name and returns */
            dir_entry[i].d_inumber = sub_inumber;
            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);
            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;

            pthread_mutex_unlock(&directory_mutex);
            return 0;
        }

    }
    /* If there were no free entries, returns an error */
    pthread_mutex_unlock(&directory_mutex);
    return -1;
}


/* Looks for a given name inside a directory
 * Input:
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {

    /* Simulate storage access delay to i-node with inumber */
    insert_delay();

    pthread_mutex_lock(&inode_table[inumber].mutex);

    if (!valid_inumber(inumber) ||
        inode_table[inumber].i_node_type != T_DIRECTORY) {
        pthread_mutex_unlock(&inode_table[inumber].mutex);
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table[inumber].i_data_block[0]); //FIXME DUVIDA : AQUI, POR EXEMPLO, PRECISO DE POR O MUTEX DA INODE TABLE?
    if (dir_entry == NULL) {
        return -1;
    }

    pthread_mutex_unlock(&inode_table[inumber].mutex);


    pthread_mutex_lock(&directory_mutex);
    /* Iterates over the directory entries looking for one that has the target
     * name */
    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {

        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {
            /* If it is found, returns the directory inumber */
            int res = dir_entry[i].d_inumber;
            pthread_mutex_unlock(&directory_mutex);
            return res;
        }

    }
    /* If it wasn't found, returns an error */
    pthread_mutex_unlock(&directory_mutex);
    return -1;
}


/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
int data_block_alloc() {

    pthread_mutex_lock(&free_blocks_mutex);

    /*Iterates over the free_blocks array searching for the first free block */
    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int) sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }
        if (free_blocks[i] == FREE) {
            free_blocks[i] = TAKEN;
            pthread_mutex_unlock(&free_blocks_mutex);
            return i;
        }
    }
    /* If there are no free blocks */
    pthread_mutex_unlock(&free_blocks_mutex);
    return -1;
}


/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    if (!valid_block_number(block_number)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks

    pthread_mutex_lock(&free_blocks_mutex);
    free_blocks[block_number] = FREE;
    pthread_mutex_unlock(&free_blocks_mutex);
    return 0;
}


/* Returns a pointer to the contents of a given block
 * Input:
 * 	- Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block

    void * res = &fs_data[BLOCK_SIZE * block_number];
    return res;
}


/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {

    pthread_mutex_lock(&free_OF_mutex);
    /* Iterates over the free_open_file_entries until it finds a free slot */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (free_open_file_entries[i] == FREE) {
            /*Takes the slot and updates the entry's values */
            free_open_file_entries[i] = TAKEN;

            pthread_mutex_lock(&open_file_table[i].mutex);
            open_file_table[i].of_inumber = inumber;
            open_file_table[i].of_offset = offset;

            pthread_mutex_unlock(&open_file_table[i].mutex);
            pthread_mutex_unlock(&free_OF_mutex);
            return i;
        }
    }
    /* If there was no free slot */
    pthread_mutex_unlock(&free_OF_mutex);
    return -1;
}


/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {
    pthread_mutex_lock(&free_OF_mutex);

    if (!valid_file_handle(fhandle) ||
        free_open_file_entries[fhandle] != TAKEN) {
        pthread_mutex_unlock(&free_OF_mutex);
        return -1;
    }
    /* Simply sets the entry to FREE */
    free_open_file_entries[fhandle] = FREE;

    pthread_mutex_unlock(&free_OF_mutex);
    return 0;
}


/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }

    open_file_entry_t * res = &open_file_table[fhandle];
    return res;
}
