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
#define sub_or_zero(nr, toSub) (toSub > nr ? 0 : nr - toSub)


void * get_next_block(int const * blocks_of_blocks, inode_t * inode, int n_blocks, int type);

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

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                int n_blocks = (int) ceil((double) inode->i_size / BLOCK_SIZE), i;

                for (i = 0; i < n_blocks && i < 10; i++)
                    if (data_block_free(inode->i_data_block[i]) == -1)
                        return -1;

                if (n_blocks > 10) { // block of blocks to be fred
                    n_blocks -= 10;
                    int *block_of_blocks = (int *) data_block_get(inode->i_data_block[10]);

                    while (n_blocks != 0) {
                        if (data_block_free(*block_of_blocks) == -1)
                            return -1;

                        block_of_blocks++;
                        n_blocks--;
                    }

                    if (data_block_free( inode->i_data_block[10]) == -1)
                        return -1;
                }

                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }

        } else if (flags & TFS_O_CREAT) {
            /* The file doesn't exist; the flags specify that it should be created */
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
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

int block_create_indirect(inode_t *inode, size_t mem){
    int n_blocks = (int) ceil((double)inode-> i_size/ BLOCK_SIZE);
    int n_blocks_ind = max(n_blocks - 10, 0); // if n_blocks becomes negative, set it to 0

    if (n_blocks_ind == 0)
        if((inode -> i_data_block[10] = data_block_alloc()) == -1) // if 0, it means the indirect block wasn't created yet
            return -1;

    int * block = (int*) data_block_get(inode ->i_data_block[10]);
    if (block == NULL)
        return -1;

    block += n_blocks_ind; // Set the pointer to the next free block position

    while (mem > 0){
        // Allocs a block in the free position and goes to next position
        *block = data_block_alloc();
        if (*block == -1)
            return -1;
        block++;
        mem = sub_or_zero(mem, BLOCK_SIZE);
    }
    return 0;
}

int block_create(inode_t * inode, size_t mem){
    int n_blocks = (int) ceil((double)inode-> i_size/ BLOCK_SIZE);
    if (n_blocks >= 10)
        return block_create_indirect(inode, mem);

    while (n_blocks != 10 && mem > 0){
        if((inode -> i_data_block[n_blocks] = data_block_alloc()) == -1)
            return -1;

        mem = sub_or_zero(mem, BLOCK_SIZE); //P Bug corrigido: size_t não tem sinal. 4 - 1024 = 18446744073709550596 e não -1020
        n_blocks++;
    }
    if (mem == 0)
        return 0;
    else if (block_create_indirect(inode,mem) == -1)
        return -1;

    return 0;
}


void * get_next_block(int const * blocks_of_blocks, inode_t * inode, int n_blocks, int type){
    if (type == DIRECT)
        return data_block_get(inode -> i_data_block[n_blocks-1]);
    else if (type == INDIRECT)
        return data_block_get(*(blocks_of_blocks + (n_blocks-1)));
}

int block_write(inode_t *inode, size_t * block_offset, char const *buffer, int *n_blocks, size_t *to_write, int to_write_cpy, size_t *mem_available, size_t *buffer_offset, int type){
    int * block_of_blocks = (int*) data_block_get(inode -> i_data_block[10]);
    void *block = get_next_block(block_of_blocks,inode,*n_blocks,type);

    if (block == NULL)
        return -1;

    size_t what_to_write = min(*mem_available, *to_write);
    memcpy(block + *block_offset, buffer + *buffer_offset, what_to_write);

    if (what_to_write == *to_write)
        return to_write_cpy;

    *buffer_offset += what_to_write;
    *to_write -= what_to_write;
    (*n_blocks)++; //P Bug: Tinhas *n_blocks++, estava a incrementar o pointer e não o conteúdo

    *block_offset = 0;
    *mem_available = 0; // nao me lembro porque fiz isto
    int value;

    if (type == DIRECT)
        value = 10;
    else if (type == INDIRECT){
        value = BLOCK_SIZE / sizeof(int);
        (*n_blocks)--;
    }

    while (*n_blocks <= value && *to_write > 0) {
        size_t what_to_write = min(*to_write, BLOCK_SIZE);
        void *block = get_next_block (block_of_blocks, inode,*n_blocks,type);

        if (block == NULL)
            return -1;

        memcpy(block + *block_offset,  buffer + *buffer_offset, what_to_write); //pode ter bugs
        *buffer_offset += what_to_write;

        if (what_to_write == *to_write)
            return to_write_cpy;

        (*n_blocks)++; //P Bug: Tinhas "n_blocks++", estavas a incrementar o pointer e não o nr de blocos
        *to_write = sub_or_zero(*to_write, BLOCK_SIZE);
    }
    return 0;
}


size_t data_block_write(inode_t * inode, size_t offset, char const * buffer, size_t to_write){
    int n_blocks = ceil((double)offset/BLOCK_SIZE); //number of blocks used. We'll try to wrote on the last one if it has remaining space
    size_t to_write_cpy = to_write, block_offset  = offset % BLOCK_SIZE,buffer_offset = 0;
    //due to the flags given, the offset only can be zero if the file is empty  or the file size
    if (block_offset == 0){ //if the block offset is 0, it means it does not have any remaining space, so we need to write on the next one
        n_blocks++;
    }
    size_t mem_available = BLOCK_SIZE - block_offset; //the memory available to write on the last block
    if (n_blocks <= 10) {
        int res = block_write(inode,&block_offset,buffer,&n_blocks,&to_write,to_write_cpy,&mem_available,&buffer_offset,DIRECT);

        if (res == -1)
            return -1;

        else if(res == to_write_cpy)
            return to_write_cpy;
    }
    //escrever indiretamente
    n_blocks -= 10;
    int res = block_write(inode,&block_offset,buffer,&n_blocks,&to_write,to_write_cpy,&mem_available,&buffer_offset,INDIRECT);

    if (res == -1)
        return -1;

    else if (res == to_write_cpy)
        return to_write_cpy;

    return -1;
    //acabar isto mas a ideia nao e dificil, e escrever no block2 e se ele acabar passar para os proximos blocos,
    //como se fez no endereçamento direto
    //acho que ja esta functional :)
}


ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /*get available memory*/
    int n_blocks = (int) ceil((double)inode -> i_size/ BLOCK_SIZE);
    size_t  mem_available = n_blocks*BLOCK_SIZE - inode -> i_size;
    if (to_write > mem_available){
        if (block_create(inode, to_write - mem_available) == -1)
            return -1;
    }

    if (to_write > 0) {

        data_block_write(inode, file -> of_offset, buffer, to_write);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    return (ssize_t)to_write;
}

/*
int block_read(){
    if (block_offset != 0 ) {
        void *block = get_next_block(block_of_blocks,inode,n_blocks,type);
        if (block == NULL)
            return -1;
        size_t what_to_read = *to_read > BLOCK_SIZE - block_offset ? BLOCK_SIZE- block_offset : to_read;
        memcpy(buffer, block, what_to_read); //FIXME VER SE O QUE E PARA LER E MENOR QUE ESTE VALOR
        buffer_offset += what_to_read;
        if(what_to_read == to_read)
            return to_read_cpy;
        n_blocks++;
        to_read -= what_to_read;
        block_offset = 0;
    }
    while (n_blocks <= 10 && to_read > 0){
        void * block = data_block_get(inode-> i_data_block[n_blocks-1]);
        if (block == NULL)
            return -1;
        size_t what_to_read = to_read > BLOCK_SIZE ? BLOCK_SIZE : to_read;
        memcpy(buffer + buffer_offset, block, what_to_read);
        buffer_offset += what_to_read;
        n_blocks++;
        if(what_to_read == to_read)
            return to_read_cpy;
        to_read-=what_to_read;
    }
}
*/

size_t data_block_read(void * buffer,inode_t *inode,size_t offset,size_t to_read){ //a ideia e saber onde comecar a ler e ler ate ao fim de cada bloco e se chegarmos ao 10 temos de ler indiretamente
    int n_blocks = (offset == 0) ? 1 :(int) ceil((double)offset/BLOCK_SIZE); //P Bug: Se offset = 0, n_blocks seria 0 e farias i_data_block[n_blocks-1] = i_data_block[-1] = BOOM //FIXME NAO TA NADA MAL MAS NAO ESQUECER QUE TEMOS DE LER PARA BUFFER + QQL COISA SENAO ESCREVEMOS ME CIMA DO QUE JA LA TAVA
    size_t block_offset = offset % BLOCK_SIZE;
    size_t buffer_offset = 0,to_read_cpy = to_read;
//FIXME MUITO IMPORTANTE : O INODE E O FILE JA FORAM LOCKED NO CONTEXTO ONDE ESTA FUNCAO FOI CHAMADA (QUE É UNICO)
    if (n_blocks <= 10) {

        if (block_offset != 0 ) {
            pthread_mutex_lock(&(inode->mutex));
            int block_number = inode -> i_data_block[n_blocks-1];
            void *block = data_block_get(inode->i_data_block[n_blocks - 1]);

            if (block == NULL)
                return -1;

            size_t what_to_read = to_read > BLOCK_SIZE - block_offset ? BLOCK_SIZE- block_offset : to_read;
            memcpy(buffer + buffer_offset, block + block_offset, what_to_read); //FIXME VER SE O QUE E PARA LER E MENOR QUE ESTE VALOR
            buffer_offset += what_to_read;

            if(what_to_read == to_read)
                return to_read_cpy;

            n_blocks++;
            to_read -= what_to_read;
            block_offset = 0;
        }

        while (n_blocks <= 10 && to_read > 0){
            void * block = data_block_get(inode-> i_data_block[n_blocks-1]);
            if (block == NULL)
                return -1;
            size_t what_to_read = to_read > BLOCK_SIZE ? BLOCK_SIZE : to_read;
            memcpy(buffer + buffer_offset, block, what_to_read);
            buffer_offset += what_to_read;
            n_blocks++;
            if(what_to_read == to_read)
                return to_read_cpy;
            to_read-=what_to_read;
        }

    }

    n_blocks -= 10;
    int* block_of_blocks = (int*)data_block_get(inode->i_data_block[10]);

    if (block_of_blocks == NULL)
        return -1;

    void * block = data_block_get(*(block_of_blocks + n_blocks - 1));

    if (block == NULL)
        return -1;

    if (block_offset != 0 ) {
        size_t what_to_read = to_read > BLOCK_SIZE - block_offset ? BLOCK_SIZE- block_offset : to_read;
        memcpy(buffer + buffer_offset, block + block_offset, what_to_read); //FIXME VER SE O QUE E PARA LER E MENOR QUE ESTE VALOR
        buffer_offset += what_to_read;

        if(what_to_read == to_read)
            return to_read_cpy;

        n_blocks++;
        to_read -= what_to_read;
        block_offset = 0;
    }

    while (to_read >= 0){
        void * block1 = data_block_get(*(block_of_blocks + n_blocks - 1));
        n_blocks++;

        size_t what_to_read = to_read > BLOCK_SIZE ? BLOCK_SIZE : to_read;
        memcpy(buffer + buffer_offset,block1,what_to_read);

        buffer_offset += what_to_read;

        if(what_to_read == to_read)
            return to_read_cpy;

        to_read-= what_to_read;
    }

    return -1;
    //fazer indiretamente; //acho que ja funciona :))
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    pthread_mutex_lock(&(file->mutex));
    inode_t *inode = inode_get(file->of_inumber);
    pthread_mutex_unlock(&(file->mutex));
    if (inode == NULL) {
        return -1;
    }
    /* Determine how many bytes to read */
    pthread_mutex_lock(&(inode->mutex));
    size_t to_read = inode->i_size - file->of_offset;
    pthread_mutex_unlock(&(inode->mutex));
    if (to_read > len) {
        to_read = len;
    }
    pthread_mutex_lock(&(file->mutex));
    size_t offset = file -> of_offset;
    pthread_mutex_unlock(&(file->mutex));
    /* Perform the actual read */
    data_block_read(buffer,inode,offset, to_read); //FIXME O MUTEX VAI SER LOCKED DENTRO DA FUNCAO
    /* The offset associated with the file handle is
     * incremented accordingly */
    pthread_mutex_lock(&(file->mutex));
    file->of_offset += to_read;
    pthread_mutex_unlock(&(file->mutex));


    return (ssize_t)to_read;
}

//-------------------------------------------------------------------------------------------------------

/*int tfs_copy_to_external_fs(char const *source_path, char const *dest_path){
    tfs_open(source_path,); //FIXME NAO SEI QUAL E A FLAG PARA ABRIR COM O CURSOR NO INICIO PORQUE O TRUNCATE APAGA TUDO

IMPLEMENTACAO USANDO AS FUNCOES DO TFS, NAO SABIA COMO ERA, VOU FAZER UMA FUNCAO QUE NAO USA ISSO, E MAIS BAIXO NIVEL
}*/

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    size_t size;
    /* Checks if the path name is valid */
    if (!valid_pathname(source_path)) {
        return -1;
    }

    int inum = tfs_lookup(source_path);
    if (inum == -1)
        return -1;

    inode_t *inode = inode_get(inum);
    if (inode == NULL)
        return -1;

    size = inode->i_size;

    char *buffer = malloc(size + 1);
    int fh = add_to_open_file_table(inum, 0);

    tfs_read(fh, buffer, size);
    tfs_close(fh);
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

