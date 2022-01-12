/*ssize_t data_block_read(void *buffer, inode_t *inode, size_t offset, size_t to_read){

    /* Gets the number of blocks from where to start to read and other calculations */
    /* If the offset is zero, then forces the number of blocks to 1 ( 0/any_number = 0) 
    int n_blocks = (offset == 0) ? 1 :(int) ceil((double)offset/BLOCK_SIZE);
    size_t block_offset = offset % BLOCK_SIZE;
    size_t buffer_offset = 0,to_read_cpy = to_read;

    /* Case where there are only direct blocks 
    if (n_blocks <= 10) {

        /* If block offset isn't zero, we want to read just a part of the starting block 
        if (block_offset != 0) {
            void *block = data_block_get(inode->i_data_block[n_blocks - 1]);

            if (block == NULL)
                return -1;

            /* The data is read and the block_offset is reseted 
            size_t what_to_read = min(to_read, BLOCK_SIZE - block_offset);
            memcpy(buffer + buffer_offset, block + block_offset, what_to_read);
            buffer_offset += what_to_read;

            /* If they match, it means everything was read, so returns 
            if(what_to_read == to_read)
                return (ssize_t) to_read_cpy;

            /* Updates the variables accordingly
            n_blocks++;
            to_read = sub_or_zero(to_read, what_to_read);
            block_offset = 0;
        }

        /* Keeps reading until the end of the direct blocks 
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
    /* So, gets the block of blocks and the right starting block 
    n_blocks -= 10;
    int* block_of_blocks = (int*)data_block_get(inode->i_data_block[10]);

    if (block_of_blocks == NULL)
        return -1;

    void *block = data_block_get(*(block_of_blocks + n_blocks-1));

    if (block == NULL)
        return -1;

    /* If block_offset isn't zero, we want to skip a part of the starting block (doesn't happen if
     * coming from direct blocks, because the block_offset is set to 0) 
    if (block_offset != 0) {

        size_t what_to_read = min(to_read, BLOCK_SIZE - block_offset);
        memcpy(buffer + buffer_offset, block + block_offset, what_to_read); //FIXME VER SE O QUE E PARA LER E MENOR QUE ESTE VALOR

        if(what_to_read == to_read)
            return (ssize_t) to_read_cpy;

        buffer_offset += what_to_read;
        n_blocks++;
        to_read = sub_or_zero(to_read, what_to_read);
    }

    /* Reads to the buffer while there are bytes to be read 
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
} */