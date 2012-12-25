#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "api.h"
#include "filesystem.h"

#define DEBUG1 0
#define DEBUG2 0

int write_block(int file, const void *buf, int block_num)
{
    lseek(file, (block_num*BLOCK_SIZE), SEEK_SET);
    return (write(file, buf, BLOCK_SIZE));
}

int read_block(int file, void *buf, int block_num)
{
    lseek(file, (block_num*BLOCK_SIZE), SEEK_SET);
    return (read(file, buf, BLOCK_SIZE));
}

int open_fs(char *fs_path)
{
    struct superblock *sb = malloc(sizeof(struct superblock));

    file = open(fs_path, O_RDWR);

    if( !read_block(file, sb, 1) )
    {
        return ERR_FILE_NOT_FOUND;
    }
    if(sb->fs_type != 12345)
    {
        return ERR_INVALID_DISK_FILE;
    }
    NUM_BLOCKS = sb->disk_size / BLOCK_SIZE;
    NUM_INODE_BLOCKS = NUM_BLOCKS / 32;
    NUM_INODES_PER_BLOCK = 8;
    NUM_INODES = NUM_INODE_BLOCKS * NUM_INODES_PER_BLOCK;
    NUM_DATA_BLOCKS = NUM_BLOCKS - 2 - NUM_INODE_BLOCKS;

    free(sb);
    return SUCCESS;
}

void close_fs()
{
    close(file);
}

int format_fs(char *fs_path, int num_blocks)
{
    int i;

    NUM_BLOCKS = num_blocks;
    NUM_INODE_BLOCKS = NUM_BLOCKS / 32;
    NUM_INODES_PER_BLOCK = 8;
    NUM_INODES = NUM_INODE_BLOCKS * NUM_INODES_PER_BLOCK;
    NUM_DATA_BLOCKS = NUM_BLOCKS - 2 - NUM_INODE_BLOCKS;

    struct bootblock *bootblock = malloc(sizeof(struct bootblock));
    struct superblock *superblock = malloc(sizeof(struct superblock));
    struct inode_block *inode_block = malloc(sizeof(struct inode_block));
    struct inode *inode = malloc(sizeof(struct inode));
    struct free_data_block *free_data_block = malloc(sizeof(struct free_data_block));
    struct directory *root_dir = malloc(sizeof(struct directory));

    if (NUM_BLOCKS < 32)
    {
        DEBUG2 && printf("Unable to create file system. Minimum blocks must be >= 32\n");
        return ERR_MIN_BLOCKS;
    }

    file = open(fs_path, O_RDWR|O_CREAT, 00777);

    // Writing the bootblock to file
    write_block(file, bootblock, 0);

    // Writing the superblock to file
    superblock->fs_type = 12345;
    superblock->disk_size = NUM_BLOCKS * BLOCK_SIZE;
    superblock->blocks_allocated = 0;
    superblock->max_blocks = NUM_DATA_BLOCKS;
    superblock->files_allocated = 1;
    superblock->max_files = NUM_INODES;
    superblock->free_inode_list = 1;
    superblock->free_data_block_list = NUM_INODE_BLOCKS + 3;
    write_block(file, superblock, 1);

    // Writing the inode block to file
    int j;
    int count = 0;

    for (j = 0; j < NUM_INODE_BLOCKS; j++)
    {
        for(i = 0; i < NUM_INODES_PER_BLOCK; i++)
        {
            if(j == 0 && i == 0)
            {
                // This is the root
                inode_block->inodes[i].is_dir = 1;
                inode_block->inodes[i].num_blocks = 1;
                //we shouldn't maintain pointers to next free inode on used inodes
                inode_block->inodes[i].next_free_inode = -2;  //j + 1;
                inode_block->inodes[i].is_free = 0;
                inode_block->inodes[i].file_blocks[0] = NUM_INODE_BLOCKS + 2;
                continue;
            }

            inode_block->inodes[i].is_dir = 0;
            inode_block->inodes[i].is_free = 1;
            inode_block->inodes[i].num_blocks = 0;
            //we dont want to point to zero since thats a valid block
            inode_block->inodes[i].file_blocks[0] = -3;
            //last free inode points to null

            if(i == NUM_INODES_PER_BLOCK-1 && j == NUM_INODE_BLOCKS-1)
                inode_block->inodes[i].next_free_inode = -1;

            else
                inode_block->inodes[i].next_free_inode = j*NUM_INODES_PER_BLOCK + i + 1;
        }

        write_block(file, inode_block, 2 + j);
    }

    // Writing the free data blocks to file
    for(i = 0; i < NUM_DATA_BLOCKS; i++)
    {
        if(i == NUM_DATA_BLOCKS - 1)
            free_data_block->next_free_block = -1;

        else
            free_data_block->next_free_block = NUM_INODE_BLOCKS + 2 + i + 1;

        write_block(file, free_data_block, NUM_INODE_BLOCKS + 2 + i);
    }

    free(superblock);
    free(inode_block);
    free(inode);
    free(free_data_block);
    free(root_dir);

    close(file);
    return SUCCESS;
}


struct inode_block *get_inode_block(int inode_num)
{
    struct inode_block *inode_blk;
    int inode_block_num  = (inode_num / 8) + 2;
    int s;

    if(inode_num < 0 || inode_num >= NUM_INODES)
    {
        DEBUG1 && printf("get_inode_block: inode_num out of range\n");
        DEBUG1 && printf("get_inode_block: inode_num = %d \n", inode_num);
        DEBUG1 && printf("get_inode_block: NUM_INODES = %d \n", NUM_INODES);
        return NULL;
    }

    inode_blk = malloc(sizeof(struct inode_block));

    if( !(read_block(file, inode_blk, inode_block_num)) )
    {
        DEBUG2 && printf("get_inode_block: failed to read inode block\n");
        free(inode_blk);
        inode_blk = NULL;
    }

    return inode_blk;
}

int put_inode_block(struct inode_block *ib, int inode_num)
{
    int inode_block_num = (inode_num / 8) + 2;

    DEBUG1 && printf("inode_block_num = %d \n", inode_block_num);

    if( !(write_block(file, ib, inode_block_num)) )
    {
        //free(inode_blk);
        //inode_blk = NULL;
        return -1;
    }

    return 0;
}

//pass 2 pointers by ref. and inode number, returns 0 on success, updates pointers by ref.
int get_inode(struct inode_block **inode_block, struct inode **inode, int inode_num)
{
    int offset = inode_num % 8;

    if(inode_num < 0)
        return -1;

    *inode_block = get_inode_block(inode_num);

    if(!inode_block)
    {
        return -1;
    }

    *inode = &(**inode_block).inodes[offset];

    return 0;
}



//find free inode, remove from free inode list, return inode number
int get_free_inode(void)
{
    struct superblock *sb = malloc(sizeof(struct superblock));
    struct inode_block *iblock;
    struct inode *inode;

    int inode_num;
    int new_free;
    int i;

    if(! read_block(file, sb, 1))
    {
        DEBUG2 && printf("Error reading superblock\n");
        return -1;
    }

    if( (inode_num = sb->free_inode_list) == -1)
    {
        DEBUG2 && printf("Error no free inodes\n");
        return -1;
    }

    if(get_inode(&iblock, &inode, inode_num))
    {
        DEBUG2 && 	printf("ERROR: couldn't get inode \n ");
        return -1;
    }

    new_free = inode->next_free_inode;

    inode->next_free_inode = -2;
    inode->is_free=0;
    inode->num_blocks=0;
    inode->is_dir=0;

    for(i=0; i < 10 ; i++)
    {
        inode->file_blocks[i] = 0;
    }

    inode->indirect1 = 0;
    inode->indirect2 = 0;


    DEBUG1 && printf("new_free: %d \n", new_free);

    sb->free_inode_list = new_free;
    sb->files_allocated++;

    if(! write_block(file, sb, 1))
    {
        DEBUG2 && printf("Error writing superblock\n");
        return -1;
    }

    if(put_inode_block(iblock, inode_num))
    {
        return -1;
    }

    free(sb);
    free(iblock);

    return inode_num;
}

int get_free_datablock(void)
{
    struct superblock *sb = malloc(sizeof(struct superblock));
    struct free_data_block *fdb = malloc(sizeof(struct free_data_block));
    struct datablock *empty_db = malloc(sizeof(struct datablock));
    int free_db_num, i;

    for(i=0; i < 512; i++)
    {
        empty_db->byte[i] = 0;
    }

    if(!read_block(file, sb, 1))
    {
        DEBUG2 && printf("Error reading superblock\n");
        return -1;
    }

    free_db_num = sb->free_data_block_list;

    if(free_db_num < 0)
    {
        free(sb);
        free(fdb);
        DEBUG1 && printf("no free data blocks \n");
        return -1;
    }

    if(!read_block(file, fdb, free_db_num))
    {
        DEBUG2 && printf("Error reading superblock\n");
        return -1;
    }

    sb->free_data_block_list = fdb->next_free_block;

    if(!write_block(file, sb, 1))
    {
        DEBUG2 && printf("Error writing superblock\n");
        return -1;
    }

    fdb->next_free_block = -1;

    if(!write_block(file, empty_db, free_db_num))
    {
        DEBUG2 && printf("Error writing new datablock\n");
        return -1;
    }

    return free_db_num;
}

int add_data_block(int inode_num)
{
    struct datablock *dblock = malloc(sizeof(struct datablock));
    struct indirection_block *iblock1;
    struct indirection_block *iblock2;
    struct inode_block *iblock;
    struct inode  *inode;

    int block_num;
    int new_db_num = get_free_datablock();
    DEBUG1 && printf("new db number: %d \n", new_db_num);

    if(new_db_num < 0)
        return -1;


    get_inode(&iblock, &inode, inode_num);

    block_num = inode->num_blocks;

    if(!inode)
    {
        DEBUG2 && printf("inode is null\n");
        return -1;
    }

    if(block_num >= 0 && block_num < 10)
    {
        inode->file_blocks[block_num] = new_db_num;
        inode->num_blocks++;

        if(put_inode_block(iblock, inode_num))
        {
            return -1;
        }

        free(iblock);

        return new_db_num;
    }

    else if(block_num == 10)
    {
        //make indirection block
        struct indirection_block *ib = malloc(sizeof(struct indirection_block));

        ib->pointer[0] = new_db_num;

        int ind_block_num = get_free_datablock();
        if(ind_block_num < 0)
            return -1;
        DEBUG1 && printf("adding indirection block: %d\n", ind_block_num);

        if(!write_block(file, ib, ind_block_num))
        {
            DEBUG2 && printf("Error writing indirection block\n");
            return -1;
        }

        inode->indirect1 = ind_block_num;
        inode->num_blocks++;

        if(put_inode_block(iblock, inode_num))
        {
            return -1;
        }

        free(ib);
        free(iblock);

        return new_db_num;
    }

    else if(block_num > 10 && block_num < (10+128))
    {
        struct indirection_block *ib = malloc(sizeof(struct indirection_block));

        int ind_block_num = inode->indirect1;

        if(!read_block(file, ib, ind_block_num))
        {
            DEBUG2 && printf("Error reading indirection block\n");
            return -1;
        }

        ib->pointer[block_num - 10] = new_db_num;

        if(! write_block(file, ib, ind_block_num))
        {
            DEBUG2 && printf("Error writing indirection block\n");
            return -1;
        }

        inode->num_blocks++;

        if(put_inode_block(iblock, inode_num))
        {
            return -1;
        }

        free(ib);
        free(iblock);

        return new_db_num;
    }

    else if(block_num == (10+128))
    {
        struct indirection_block *ib1 = malloc(sizeof(struct indirection_block));
        struct indirection_block *ib2 = malloc(sizeof(struct indirection_block));

        int ind_block_num1 = get_free_datablock();
        int ind_block_num2 = get_free_datablock();

        if(ind_block_num1 < 0 || ind_block_num2 < 0)
            return -1;

        inode->indirect2 = ind_block_num1;
        inode->num_blocks++;

        ib1->pointer[0] = ind_block_num2;
        ib2->pointer[0] = new_db_num;

        if(!write_block(file, ib1, ind_block_num1))
        {
            DEBUG2 && printf("Error writing indirection block\n");
            return -1;
        }

        if(!write_block(file, ib2, ind_block_num2))
        {
            DEBUG2 && printf("Error writing indirection block\n");
            return -1;
        }

        if(put_inode_block(iblock, inode_num))
        {
            return -1;
        }

        free(ib1);
        free(ib2);
        free(iblock);

        return new_db_num;
    }

    else if(block_num > (10+128) && block_num < (10+128+(128*128)))
    {
        struct indirection_block *ib1 = malloc(sizeof(struct indirection_block));
        struct indirection_block *ib2 = malloc(sizeof(struct indirection_block));

        int ind_block_num1 = inode->indirect2;

        if( !read_block(file, ib1, ind_block_num1))
        {
            DEBUG2 && printf("error reading datablock\n");
            return -1;
        }

        if(((block_num - (10+128)) % 128) == 0)
        {
            int new_ind_block = get_free_datablock();
            ib1->pointer[(block_num - (10+128)) / 128] = new_ind_block;
            ib2->pointer[0] = new_db_num;
            inode->num_blocks++;

            if(!write_block(file, ib1, ind_block_num1))
            {
                DEBUG2 && printf("error reading datablock\n");
                return -1;
            }

            if(!write_block(file, ib2, new_ind_block))
            {
                DEBUG2 && printf("error reading datablock\n");
                return -1;
            }
            if(put_inode_block(iblock, inode_num))
            {
                return -1;
            }

            return new_db_num;
        }

        else
        {
            int ind_block_num2 = ib1->pointer[(block_num - (10+128)) / 128];

            if(!read_block(file, ib2, ind_block_num2))
            {
                DEBUG2 && printf("error reading datablock\n");
                return -1;
            }

            ib2->pointer[(block_num - (10+128)) % 128] = new_db_num;

            inode->num_blocks++;

            if( !write_block(file, ib2, ind_block_num2))
            {
                DEBUG2 && printf("error reading datablock\n");
                return -1;
            }

            if(put_inode_block(iblock, inode_num))
            {
                return -1;
            }

            return new_db_num;
        }
    }

    else
    {
        DEBUG2 && printf("Error: cannot add another block, filesize max reached\n");
        return -1;
    }

    return new_db_num;
}

int add_dir_to_inode(int inode_num, char *n_dir, int n_inode_num)
{
    struct inode_block *ib = malloc(sizeof(struct inode_block));
    struct inode *inode = NULL;
    struct directory *nd = malloc(sizeof(struct directory));
    struct directory *cur_dir = NULL;
    struct datablock *datablock = NULL;


    int data_block_num;
    int i;
    int new_dir_block_num;
    for(i=0 ; i < 32 ; i++)
    {
        strcpy(nd->entries[i].filename,"");
        nd->entries[i].inode_number = 0;
    }

    get_inode(&ib, &inode, inode_num);

    if(inode->num_blocks == 0)
    {
        new_dir_block_num = add_data_block(inode_num);

        if(new_dir_block_num < 0)
        {
            return ERR_DISK_FULL;
        }

        strcpy(nd->entries[0].filename, n_dir);
        nd->entries[0].inode_number = n_inode_num;

        if(!write_block(file, nd, new_dir_block_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            return ERR_INTERNAL;
        }
        return SUCCESS;
    }

    if(inode->num_blocks > 0)
    {
        //int dir_block_num = inode->num_blocks - 1;
        data_block_num = get_data_block(&datablock, inode, inode->num_blocks - 1);
        cur_dir = (struct directory *)datablock;

        for(i=0; i < 32 ; i++)
        {
            if(cur_dir->entries[i].inode_number <= 0)
            {
                cur_dir->entries[i].inode_number = n_inode_num;
                strcpy(cur_dir->entries[i].filename, n_dir);

                if( !write_block(file, cur_dir, data_block_num))
                {
                    DEBUG2 && printf("error reading datablock\n");
                    return ERR_INTERNAL;
                }
                return SUCCESS;
            }
        }
        //we got here, we need another data block
        new_dir_block_num = add_data_block(inode_num);
        if(new_dir_block_num < 0)
        {
            DEBUG2 && printf("error no more free data blocks\n");
            return ERR_DISK_FULL;
        }
        strcpy(nd->entries[0].filename, n_dir);
        nd->entries[0].inode_number = n_inode_num;

        if(!write_block(file, nd, new_dir_block_num))
        {
            DEBUG2 && printf("error writing datablock\n");
            return ERR_INTERNAL;
        }
        return SUCCESS;

    }

    return SUCCESS;
}

int get_data_block(struct datablock **dblk, struct inode *inode, int file_block_num)
{
    struct datablock *dblock = malloc(sizeof(struct datablock));
    struct indirection_block *iblock1;
    struct indirection_block *iblock2;

    int block_num;

    if(!inode)
    {
        DEBUG2 && printf("inode is null\n");
        dblock = NULL;
        return -1;
    }

    if(file_block_num >= 0 && file_block_num < 10)
    {
        block_num = inode->file_blocks[file_block_num];

        if(!read_block(file, dblock, block_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            dblock = NULL;
            return -1;
        }
    }

    else if(file_block_num >= 10 && file_block_num < (10+128))
    {
        iblock1 = malloc(sizeof(struct indirection_block));
        block_num = inode->indirect1;

        if(!read_block(file, iblock1, block_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            //return NULL;
            dblock = NULL;
            return -1;

        }

        block_num = iblock1->pointer[file_block_num - 10];

        if(!read_block(file, dblock, block_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            //return NULL;
            dblock = NULL;
            return -1;
        }

        free(iblock1);
    }

    else if(file_block_num >= (10+128) && file_block_num < (10+128+(128*128)))
    {
        iblock1 = malloc(sizeof(struct indirection_block));
        iblock2 = malloc(sizeof(struct indirection_block));

        block_num = inode->indirect2;

        if(!read_block(file, iblock1, block_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            //return NULL;
            dblock = NULL;
            return -1;
        }

        block_num = iblock1->pointer[(file_block_num - (10+128)) / 128 ];

        if( !read_block(file, iblock2, block_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            //return NULL;
            dblock = NULL;
            return -1;
        }

        block_num = iblock2->pointer[(file_block_num - (10+128)) % 128];

        if( !read_block(file, dblock, block_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            //return NULL;
            dblock = NULL;
            return -1;

        }

        free(iblock1);
        free(iblock2);
    }

    else
    {
        DEBUG2 && printf("Error: block number out of range\n");
        // return NULL;
        dblock = NULL;
        return -1;
    }

    *dblk = dblock;

    return block_num;
}

int has_file(struct inode *cur_inode, char *cur)
{
    //this assumes cur_inode is dir and looks for something named cur
    int i,k;

    struct directory *cur_directory_block = NULL;// = malloc(sizeof(struct directory));
    struct datablock *datablock = NULL;

    if(cur_inode == NULL)
    {
        DEBUG1 && printf("cur_inode is null \n");
        return -1;
    }

    if(cur_inode->is_dir == 0)
    {
        return -2; //cur_inode not dir
    }

    for(i=0 ; i < cur_inode->num_blocks; i++)
    {
        get_data_block(&datablock, cur_inode, i);
        cur_directory_block = (struct directory *)datablock;

        if( ! (cur_directory_block) )
        {
            DEBUG2 && printf("error reading dir block\n");
            return -2;
        }

        for(k=0 ; k < 32 ; k++)
        {
            int cur_inum = cur_directory_block->entries[k].inode_number;
            char *cur_iname = cur_directory_block->entries[k].filename;

            //printf("has_file: cur_inum %d\n",cur_inum);
            //printf("has_file: cur_iname %s %d\n", cur_iname, strlen(cur_iname));
            //printf("has_file: cur %s %d\n", cur, strlen(cur));
            if(cur_inum < 0)
            {
                return -2;//invalid inode
            }

            if(strcmp(cur, cur_iname) == 0)
            {
                //printf("has_file: match\n");
                return cur_inum;
            }

        }
    }

    return -1; //not found
}

int hack_funct()
{
    struct inode_block *ib = NULL;
    struct inode *inode = NULL;
    struct directory *new_d = malloc(sizeof(struct directory));

    int i, j;

    get_inode(&ib, &inode, 1);

    inode->next_free_inode = -1;
    inode->is_free = 0;
    inode->is_dir = 1;
    inode->num_blocks = 1;
    inode->file_blocks[0] = 4;

    write_block(file, ib, 2);
    free(ib);

    for (i=0; i<32; i++)
    {
        for (j=0; j<12; j++)
        {
            new_d->entries[i].filename[j] = 0;
        }

        new_d->entries[i].inode_number = 0;
    }


    new_d->entries[0].filename[0] = 'c';
    new_d->entries[0].filename[1] = 'w';
    new_d->entries[0].filename[2] = 'i';
    new_d->entries[0].filename[3] = 'l';
    new_d->entries[0].filename[4] = 'l';
    new_d->entries[0].filename[5] = 's';
    new_d->entries[0].filename[6] = '\0';

    new_d->entries[0].inode_number = 2;

    write_block(file, new_d, 4);

    free(new_d);
    //printf("endofhack\n");
}

int create_file(char *path, int is_dir)
{
    char *lpath = malloc(sizeof(char)*strlen(path)+1);
    char *last;
    char *ptr;
    int wd, free_inode_num, found, s;

    struct inode_block *cur_inode_block = NULL;
    struct inode *cur_inode = NULL;

    strcpy(lpath, path);

    if(strlen(lpath) == 1)
    {
        if(lpath[0] == '/')
        {
            return ERR_FILE_EXISTS;
        }
        else
        {
            return ERR_INVALID_PATH;
        }
    }

    ptr = strrchr(lpath, '/');


    if(ptr == lpath+strlen(lpath)-1)
    {
        *ptr = '\0';
        ptr = strrchr(lpath, '/');
    }

    *ptr = '\0';
    ptr++;

    last = malloc(sizeof(char)*strlen(ptr)+1);

    strcpy(last, ptr);

    wd = path_to_inode(lpath);

    DEBUG1 &&  printf("wd = %d \n", wd);

    if(wd < 0)
    {
        return ERR_INVALID_PATH;
    }
    //printf("wd inode = %d \n", wd);

    get_inode(&cur_inode_block, &cur_inode, wd);

    if(cur_inode_block == NULL || cur_inode == NULL)
    {
        DEBUG2 && 	printf("get inode failed\n");
        return ERR_INTERNAL;
    }

    found = has_file(cur_inode, last);
    //printf("found = %d \n", found);
    DEBUG1 && printf("found = %d \n", found);

    if(found != -1)
    {
        //printf("file or dir already exists!\n");
        //printf("wd = %d, last = %s \n ", wd, last);
        return ERR_FILE_EXISTS;
    }

    free_inode_num = get_free_inode();
    //printf("free inode num = %d\n", free_inode_num);

    if(free_inode_num < 0)
    {
        return ERR_MAX_FILES;
    }

    get_inode( &cur_inode_block, &cur_inode , free_inode_num);

    if(cur_inode_block == NULL || cur_inode == NULL)
    {
        return ERR_INTERNAL;
    }

    cur_inode->is_dir = is_dir;

    put_inode_block(cur_inode_block, free_inode_num);

    //free_data_block_num = get_free_datablock();
    //printf("free_data_block_num = %d\n", free_data_block_num);

    s = add_dir_to_inode(wd, last, free_inode_num);

    return s;
}

int path_to_inode(char *orig_path)
{
    char *cur;
    struct inode_block *cur_inode_block = NULL;
    struct inode *cur_inode = NULL;
    char *path;
    path = malloc(sizeof(char)*strlen(orig_path));
    strcpy(path, orig_path);

    pwd = 0; //start at root

    //printf("path = %s\n");
    //if(strcmp(path, "/") == 0)
    //return 0;

    for(cur = strtok(path, "/"); cur != NULL; cur = strtok(NULL, "/"))
    {
        int prev_wd = pwd;

        DEBUG1 && printf("%s \n",cur);

        if(get_inode(&cur_inode_block, &cur_inode, pwd))
        {
            DEBUG2 && printf("ERROR: couldn't get free inode \n ");
            return -1;
        }

        pwd = has_file(cur_inode, cur);

        DEBUG1 && printf("pwd = %d\n", pwd);

        if(pwd == -1)
        {
            DEBUG2 && printf("not found or not valid inode\n");
            //pwd = prev_wd;
            pwd = -1;
            break;
        }

        if(pwd == -2)
        {
            DEBUG1 && printf("not a dir\n");
            //pwd = prev_wd;
            pwd = -1;
            break;
        }

    }
    free(path);
    return pwd;
}

int file_open(char *pathOf)
{
    struct inode_block *inode_block = NULL;
    struct inode *inode = NULL;
    int i;
    int inum = path_to_inode(pathOf);
    get_inode(&inode_block, &inode, inum);

    // Check if the file exists. If not then error out
    if (inum == -1)
    {
        DEBUG2 && printf("File not found\n");
        return ERR_FILE_NOT_FOUND;
    }

    get_inode(&inode_block, &inode, inum);

    if (inode->is_dir == 1)
    {
        DEBUG2 && printf("The file is a directory\n");
        return ERR_FILE_NOT_FOUND;
    }

    DEBUG1 && printf("File does exist, is not a directory and is in inode number %d\n", inum);

    //allowing multiple opens of the same file so we need not check if a file is already opened
    //we just add whatever file we get whether its been opened alread or not
    /*
    for (i = 0; i < 20; i++) {
    	if (open_file_table[i].currently_opened == 1 && open_file_table[i].inode_number == inum){
    		printf("File already opened\n");
    		return ERR_FILE_ALREADY_OPEN;
    	}
    }
    */

    DEBUG1 && printf("Adding to open file table\n");

    // If file exists and is not open, then add an entry to the open file table

    for (i = 0; i < 20; i++)
    {
        if (open_file_table[i].currently_opened == 0)
        {
            open_file_table[i].inode_number = inum;
            open_file_table[i].currently_opened = 1;
            open_file_table[i].seek_position = 0;
            DEBUG1 && printf("File added to open file table\n");
            return i; //this is the index in the table were we put inode
        }
    }

    DEBUG1 && printf("Maximum files opened. Cannot open file\n");
    return ERR_TOO_MANY_FILES_OPEN;
}

void file_close(int file_number)
{

    //all we have to do is remove it from the table if its there
    if(open_file_table[file_number].currently_opened == 0)
    {
        DEBUG1 && printf("file with descriptor %d is not currently open. \n", file_number);
        return;
    }
    else
    {
        open_file_table[file_number].currently_opened = 0;
        return;
    }


    /*
    struct inode_block *inode_block = NULL;
    struct inode *inode = NULL;
    int inum = path_to_inode(pathOf);

    get_inode(&inode_block, &inode, inum);

    // Check if the file exists. If not then error out
    if (inum == -1)
    {
    	printf("File not found\n");
    	return ERR_FILE_NOT_FOUND;
    }

    if (inode->is_dir == 1)
    {
    	printf("The file is a directory\n");
    	return -2;
    }

    printf("File does exist, is not a directory and is in inode number %d\n", inum);

    int i;

    // Check if file is already opened. If yes the close it
    for (i = 0; i < 20; i++)
    {
    	if (open_file_table[i].inode_number == inum){
    		open_file_table[i].currently_opened = 0;
    		printf("File closed\n");
    		return SUCCESS;
    	}
    }

    printf("File is not opened. Cannot be removed from open file table\n");
    */
}

int file_write(int file_number, void *buffer, int bytes)
{
    struct inode_block *inode_block = NULL;
    struct inode *inode = NULL;
    struct datablock *datablock = NULL;
    int copened, inum, spos;
    int file_size; //in bytes
    int pos, bnum, bidx;
    int new_db = 0;
    int bytes_w =0;
    BYTE *bbuffer = (BYTE *)buffer;
    int cur_blk_num;

    copened = open_file_table[file_number].currently_opened;
    inum = open_file_table[file_number].inode_number;
    spos = open_file_table[file_number].seek_position;

    if(copened == 0)
    {
        DEBUG1 && printf("file desc %d is not opened \n", file_number);
        return ERR_FILE_NOT_OPEN;
    }
    if(inum <= 0)
    {
        DEBUG2 && printf("invalid inode \n");
        return ERR_INTERNAL;
    }

    get_inode(&inode_block, &inode, inum);

    file_size = 512 * inode->num_blocks;

    //printf("num_data_blocks = %d \n", inode->num_blocks);
    //printf("file_size = %d \n", file_size);

    while(((512*inode->num_blocks) - spos) < bytes)
    {
        int new_db_num;

        new_db_num = add_data_block(inum);
        get_inode(&inode_block, &inode, inum);
        if(new_db_num <=0)
        {
            //cannot get anymore blocks!! must be out of space
            //we will write what we can
            break;
        }

        new_db++;
    }

    //printf("num_data_blocks = %d \n", inode->num_blocks);

    bnum = spos / 512;
    bidx = spos % 512;
    //printf("bum %d, bidx %d \n",bnum, bidx);

    //printf("starting byte = %d\n", datablock->byte[bidx]);

    int i = bidx;
    pos = spos;
    while(bytes_w < bytes && pos < inode->num_blocks*512)
    {
        cur_blk_num = get_data_block(&datablock, inode, bnum);

        if(datablock == NULL)
        {
            DEBUG2 && printf("error: datablock is null \n");
            break;
        }

        for(i; i < 512; i++)
        {
            datablock->byte[i] = *bbuffer;
            bbuffer++;
            bytes_w++;
            pos++;
            if(bytes_w >= bytes || pos >= inode->num_blocks*512)
            {
                break;
            }
        }
        if( !write_block(file, datablock, cur_blk_num))
        {
            DEBUG2 && printf("error reading datablock\n");
            return ERR_INTERNAL;
        }

        //new_db--;
        i = 0;
        bnum++;
        //cur_blk_num = get_data_block(&datablock, inode, bnum);
    }

    open_file_table[file_number].seek_position += bytes_w;

    return bytes_w;

}

int file_read(int file_number, void *buffer, int bytes)
{
    struct inode_block *inode_block = NULL;
    struct inode *inode = NULL;
    struct datablock *datablock = NULL;
    int copened, inum, spos;
    int file_size; //in bytes
    int pos, bnum, bidx;
    int new_db = 0;
    int bytes_r=0;
    BYTE *bbuffer = (BYTE *)buffer;
    int cur_blk_num;

    copened = open_file_table[file_number].currently_opened;
    inum = open_file_table[file_number].inode_number;
    spos = open_file_table[file_number].seek_position;

    if(copened == 0)
    {
        DEBUG1 && printf("file desc %d is not opened \n", file_number);
        return ERR_FILE_NOT_OPEN;
    }
    if(inum <= 0)
    {
        DEBUG2 && printf("invalid inode \n");
        return ERR_INTERNAL;
    }

    get_inode(&inode_block, &inode, inum);

    file_size = 512 * inode->num_blocks;

    //printf("num_data_blocks = %d \n", inode->num_blocks);
    //printf("file_size = %d \n", file_size);

    bnum = spos / 512;
    bidx = spos % 512;
    //printf("bum %d, bidx %d \n",bnum, bidx);

    //printf("starting byte = %d\n", datablock->byte[bidx]);

    int i = bidx;
    while(bytes_r < bytes && (file_size - (spos+bytes_r)) > 0)
    {
        if(bnum > inode->num_blocks)
            break;

        cur_blk_num = get_data_block(&datablock, inode, bnum);

        if(datablock == NULL)
        {
            DEBUG2 && printf("error: datablock is null \n");
            break;
        }

        for(i; i < 512; i++)
        {
            *bbuffer = datablock->byte[i];
            bbuffer++;
            bytes_r++;
            if(bytes_r >= bytes || (file_size - (spos+bytes_r)) < 0 )
            {
                break;
            }
        }

        //new_db--;
        i = 0;
        bnum++;
        //cur_blk_num = get_data_block(&datablock, inode, bnum);
    }

    open_file_table[file_number].seek_position += bytes_r;

    return bytes_r;

}

int file_lseek(int file_number, int offset, int command)
{
    struct inode_block *inode_block = NULL;
    struct inode *inode = NULL;
    struct datablock *datablock = NULL;
    int copened, inum, spos;
    int file_size; //in bytes
    int pos, bnum, bidx;
    int new_db = 0;
    int cur_blk_num;
    int new_seek=0;
    int db_needed=0;

    copened = open_file_table[file_number].currently_opened;
    inum = open_file_table[file_number].inode_number;
    spos = open_file_table[file_number].seek_position;

    if(copened == 0)
    {
        DEBUG1 && printf("LSEEK ERROR: file desc %d is not opened \n", file_number);
        return ERR_FILE_NOT_OPEN;
    }
    if(inum <= 0)
    {
        DEBUG2 && printf("LSEEK ERROR: invalid inode \n");
        return ERR_INTERNAL;
    }

    get_inode(&inode_block, &inode, inum);

    file_size = 512 * inode->num_blocks;

    if(command == LSEEK_FROM_CURRENT)
    {
        new_seek = spos + offset;
    }
    else if(command == LSEEK_ABSOLUTE)
    {
        new_seek = offset;
        DEBUG1 && printf("newseek = %d \n", new_seek);
    }
    else if(command == LSEEK_END)
    {
        new_seek = file_size + offset;
    }
    else
    {
        DEBUG1 && printf("error: invalid lseek command\n");
        return ERR_INVALID_LSEEK_CMD;

    }

    if(new_seek >= 0 && new_seek <= file_size)
    {
        open_file_table[file_number].seek_position = new_seek;
        return new_seek;
    }
    else if(new_seek > file_size)
    {
        db_needed = (new_seek / 512) - inode->num_blocks;

        while(db_needed > 0)
        {
            int new_db_num;

            new_db_num = add_data_block(inum);
            get_inode(&inode_block, &inode, inum);
            if(new_db_num <=0)
            {
                //cannot get anymore blocks!! must be out of space
                //we will write what we can
                break;
            }
            db_needed--;
        }

        if(db_needed == 0)
        {
            open_file_table[file_number].seek_position = new_seek;
            return new_seek;
        }
        else
        {
            new_seek = (inode->num_blocks-1)*512 + 511;
            return new_seek;
        }
    }
    else
    {
        DEBUG2 && printf("error invalid offset \n");
        return ERR_INVALID_LSEEK_OFFSET;
    }

    //printf("num_data_blocks = %d \n", inode->num_blocks);
    //printf("file_size = %d \n", file_size);

    //bnum = spos / 512;
    //bidx = spos % 512;
}

int file_create(char *path)
{
    int ret;
    ret = create_file((path), 0);
    return ret;
}

int file_mkdir(char *path)
{
    int ret;
    ret = create_file((path), 1);
    return ret;
}



int erase_inode(int inode_num)
{
    struct inode_block *ib = NULL;
    struct inode *inode = NULL;
    struct directory *dir = NULL;
    struct datablock *datablock = NULL;
    struct indirection_block *idb = malloc(sizeof(struct indirection_block));
    struct indirection_block *idb2 = malloc(sizeof(struct indirection_block));
    struct superblock *sb = malloc(sizeof(struct superblock));
    int cur_db_num, i;

    if((get_inode(&ib, &inode, inode_num)) < 0)
    {
        DEBUG2 && printf("error get_inode\n");
        return -1;
    }

    while(inode->num_blocks > 0)
    {
        cur_db_num = get_data_block(&datablock, inode, inode->num_blocks-1);
        make_free_datablock(cur_db_num);
        inode->num_blocks--;
    }
    if(inode->indirect1 != 0)
    {
        make_free_datablock(inode->indirect1);
        inode->indirect1 = 0;
    }
    if(inode->indirect2 !=0)
    {
        if(!read_block(file, idb2, inode->indirect2))
        {
            return ERR_INTERNAL;
        }
        for(i=0; i<128; i++)
        {
            if(idb2->pointer[i] != 0)
            {
                make_free_datablock(idb2->pointer[i]);
            }
        }
        make_free_datablock(inode->indirect2);
        inode->indirect2 = 0;
    }
    for(i=0; i<10; i++)
    {
        inode->file_blocks[i] = 0;
    }
    inode->is_dir = 0;
    inode->is_free = 1;

    if(!read_block(file, sb, 1))
    {
        return ERR_INTERNAL;
    }

    inode->next_free_inode = sb->free_inode_list;
    sb->free_inode_list = inode_num;

    if(!write_block(file, sb, 1))
    {
        return ERR_INTERNAL;
    }
    put_inode_block(ib, inode_num);

}

int trim_indirection_blocks(int inode_num)
{
    struct inode_block *ib = NULL;
    struct inode *inode = NULL;
    struct indirection_block *idb = malloc(sizeof(struct indirection_block));
    int num_blocks;



    if((get_inode(&ib, &inode, inode_num)) < 0)
    {
        DEBUG2 && printf("error get_inode\n");
        return -1;
    }

    num_blocks = inode->num_blocks;


    if(num_blocks < 10 && inode->indirect1 != 0)
    {
        //remove inode->indirect1
        make_free_datablock(inode->indirect1);
        inode->indirect1 = 0;
        put_inode_block(ib, inode_num);
        return 1;
    }

    if(num_blocks < (10+128) && inode->indirect2 != 0)
    {
        //remove inode->indirect2
        make_free_datablock(inode->indirect2);
        inode->indirect2 = 0;
        put_inode_block(ib, inode_num);
        return 1;
    }

    if(num_blocks >= (10+128) && num_blocks < (10+128+(128*128)))
    {
        if(((num_blocks - (10+128)) % 128) == 0)
        {
            //remove second level indirection block
            if(!read_block(file, idb, inode->indirect2))
            {
                DEBUG2 && printf("Error reading indirection block\n");
                return -1;
            }
            if( idb->pointer[(num_blocks - (10+128)) / 128] != 0)
            {
                make_free_datablock(idb->pointer[(num_blocks - (10+128)) / 128]);
                idb->pointer[(num_blocks - (10+128)) / 128] = 0;
                return 1;
            }
            //block to remove
            //ib1->pointer[(block_num - (10+128)) / 128]

            //int new_ind_block = get_free_datablock();
            //ib1->pointer[(block_num - (10+128)) / 128]
            //ib2->pointer[0] = new_db_num;
            //inode->num_blocks++;
        }
    }
    return 0;

}

int delete_file(char *path)
{
    char *lpath = malloc(sizeof(char)*strlen(path)+1);
    char *orig_path = malloc(sizeof(char)*strlen(path)+1);
    char *last;
    char *ptr;
    char null_array[12];
    int wd, free_inode_num, found, s;
    int inode_num;
    struct inode_block *ib = NULL;
    struct inode *inode = NULL;
    struct inode_block *doomed_ib = NULL;
    struct inode *doomed_inode = NULL;
    struct directory *dir = NULL;
    struct datablock *datablock = NULL;
    struct directory *ldir = NULL;
    struct datablock *ldatablock = NULL;
    int doomed_inode_num;
    int i, j, k, foundit = 0;
    int cur_db_num, last_db_num;

    /*
      for(i=0;i<12;i++){
        null_array[i] = '\0';
      }
      */

    strcpy(lpath, path);
    strcpy(orig_path, path);

    if(strlen(lpath) == 1)
    {
        if(lpath[0] == '/')
        {
            return ERR_FILE_EXISTS; //update this to correct val
        }
        else
        {
            return ERR_INVALID_PATH;
        }
    }

    ptr = strrchr(lpath, '/');


    if(ptr == lpath+strlen(lpath)-1)
    {
        *ptr = '\0';
        ptr = strrchr(lpath, '/');
    }

    ptr++;

    last = malloc(sizeof(char)*strlen(ptr)+1);

    strcpy(last, ptr);

    *ptr = '\0';

    wd = path_to_inode(lpath);

    DEBUG1 && printf("lpath = %s last = %s \n", lpath, last);

    //funct to remove all data blocks

    if((doomed_inode_num = path_to_inode(orig_path)) < 0)
    {
        DEBUG1 && printf("cannot delete, does not exist\n");
        return ERR_FILE_NOT_FOUND;
    }

    if((get_inode(&doomed_ib, &doomed_inode, doomed_inode_num)) < 0)
    {
        DEBUG2 && printf("error get_inode\n");
        return -1;
    }

    if(doomed_inode->is_dir != 0 && doomed_inode->num_blocks > 0)
    {
        DEBUG1 && printf("cannot delete a none empty directory");
        return ERR_INVALID_PATH;//should be a different error
    }

    erase_inode(doomed_inode_num);


    //funct to remove from dir

    if((inode_num = path_to_inode(lpath)) < 0)
    {
        DEBUG2 && printf("error path_to_inode\n");
        return -1;
    }

    if((get_inode(&ib, &inode, inode_num)) < 0)
    {
        DEBUG2 && printf("error get_inode\n");
        return -1;
    }

    for(i=0; i<inode->num_blocks; i++)
    {
        cur_db_num = get_data_block(&datablock, inode, i);
        dir = (struct directory *)datablock;
        for(j=0; j<32; j++)
        {
            if(strcmp(last,dir->entries[j].filename) == 0 )
            {
                DEBUG1 && printf("got it!\n");
                //strcpy(dir->entries[j].filename, "");
                //dir->entries[j].inode_number = 0;
                foundit = 1;
                //write_block(file, dir, cur_db_num);
                break;
            }
        }
        if(foundit)
        {
            break;
        }
    }

    if(!foundit)
    {
        return -1;
    }

    last_db_num = get_data_block(&ldatablock, inode, inode->num_blocks-1);
    ldir = (struct directory *)ldatablock;


    if(last_db_num == cur_db_num)
    {
        for(k=0; k<32; k++)
        {
            if(dir->entries[k].inode_number == 0)
            {
                //k++;
                break;
            }
        }
        // k-1 is the one we want, even if we went all the way to the end.
        strcpy(dir->entries[j].filename, dir->entries[k-1].filename);
        dir->entries[j].inode_number = dir->entries[k-1].inode_number;
        for(i=0; i<12; i++)
        {
            dir->entries[k-1].filename[i] = '\0';
        }
        dir->entries[k-1].inode_number = 0;
        write_block(file, dir, cur_db_num);

        if(k-1 == 0)
        {
            //add last_db_num to free blocks
            if(make_free_datablock(last_db_num))
            {
                DEBUG2 && printf("couldnt free datablock\n");
            }
            //subtract a datablock from inode->num_blocks
            //WE NEED TO ADD STUFF TO SUBTRACT INDIRECTION BLOCKS
            inode->num_blocks--;
            DEBUG1 && printf("inode->num_blocks = %d \n", inode->num_blocks);
            //write the inode back
            put_inode_block(ib, inode_num);
            trim_indirection_blocks(inode_num);
        }

    }
    else
    {

        for(k=0; k<32; k++)
        {
            if(ldir->entries[k].inode_number == 0)
            {
                //k++;
                break;
            }
        }
        // k-1 is the one we want, even if we went all the way to the end.
        strcpy(dir->entries[j].filename, ldir->entries[k-1].filename);
        dir->entries[j].inode_number = ldir->entries[k-1].inode_number;
        for(i=0; i<12; i++)
        {
            ldir->entries[k-1].filename[i] = '\0';
        }
        ldir->entries[k-1].inode_number = 0;

        write_block(file, dir, cur_db_num);
        write_block(file, ldir, last_db_num);
        if(k-1 == 0)
        {
            //add last_db_num to free blocks
            if(make_free_datablock(last_db_num))
            {
                DEBUG1 && printf("couldnt free datablock\n");
            }
            //subtract a datablock from inode->num_blocks
            //WE NEED TO ADD STUFF TO SUBTRACT INDIRECTION BLOCKS
            inode->num_blocks--;
            DEBUG1 && printf("inode->num_blocks = %d \n", inode->num_blocks);
            //write the inode back
            put_inode_block(ib, inode_num);
            trim_indirection_blocks(inode_num);
        }
    }
    return SUCCESS;
}

int make_free_datablock(int db_num)
{
    struct superblock *sb = malloc(sizeof(struct superblock));
    struct free_data_block *fdb = malloc(sizeof(struct free_data_block));
    //struct datablock *empty_db = NULL;

    int i;

    fdb->next_free_block = 0;
    for(i=0; i < 508; i++)
    {
        fdb->pad[i] = 0;
    }

    if(!read_block(file, sb, 1))
    {
        DEBUG2 && printf("Error reading superblock\n");
        return -1;
    }

    //free_db_num = sb->free_data_block_list;

    /*
    if(free_db_num < 0){
    		free(sb);
    		free(fdb);
    		printf("no free data blocks \n");
    		return -1;
    }
    */
    /*
    if(!read_block(file, fdb, free_db_num)){
    		printf("Error reading superblock\n");
    		return -1;
    }
    */
    fdb->next_free_block = sb->free_data_block_list;
    sb->free_data_block_list = db_num;

    if(!write_block(file, fdb, db_num))
    {
        DEBUG2 && printf("Error writing new datablock\n");
        return -1;
    }

    if(!write_block(file, sb, 1))
    {
        DEBUG2 && printf("Error writing superblock\n");
        return -1;
    }

    //fdb->next_free_block = -1;

    return SUCCESS;
}


int file_delete(char *path)
{
    struct inode_block *ib = NULL;
    struct inode *inode = NULL;
    int inode_num;
    if((inode_num = path_to_inode(path)) < 0)
    {
        DEBUG2 && printf("error path_to_inode\n");
        return ERR_INTERNAL;
    }
    if((get_inode(&ib, &inode, inode_num)) < 0)
    {
        DEBUG2 && printf("error get_inode\n");
        return ERR_INTERNAL;
    }
    if(inode->is_dir != 0)
    {
        DEBUG2 && printf("error not a file!\n");
        return ERR_NOT_A_FILE;
    }
    return delete_file(path);

}

int file_rmdir(char *path)
{
    struct inode_block *ib = NULL;
    struct inode *inode = NULL;
    int inode_num;
    if((inode_num = path_to_inode(path)) < 0)
    {
        DEBUG2 && printf("error path_to_inode\n");
        return ERR_INTERNAL;
    }
    if((get_inode(&ib, &inode, inode_num)) < 0)
    {
        DEBUG2 && printf("error get_inode\n");
        return ERR_INTERNAL;
    }
    if(inode->is_dir != 1)
    {
        DEBUG2 && printf("error not a dir!\n");
        return ERR_NOT_A_DIR;
    }
    return delete_file(path);

}

char **file_listdir(char *path)
{
    struct inode_block *ib = NULL;
    struct inode *inode = NULL;
    //struct directory *nd = malloc(sizeof(struct directory));
    struct directory *cur_dir = NULL;
    struct datablock *datablock = NULL;
    int inode_num, s, i,j;
    char **array;
    char *word;
    char *last;
    int counter;

    last = malloc(sizeof(char)*2);
    strcpy(last, "");

    inode_num = path_to_inode(path);

    if(inode_num < 0)
    {
        return NULL;
    }

    s = get_inode(&ib, &inode, inode_num);
    if(s < 0)
    {
        return NULL;
    }

    if(inode->num_blocks == 0)
    {
        array = malloc(sizeof(char *)*2);
        array[0] = last;
        return array;
    }

    array = malloc((sizeof(char *)*32*inode->num_blocks)+1) ;
    counter = 0;
    for(i=0; i < inode->num_blocks; i++)
    {
        get_data_block(&datablock, inode, i);
        cur_dir = (struct directory *)datablock;
        for(j=0; j<32 ; j++)
        {
            if(cur_dir->entries[j].inode_number > 0)
            {
                word = malloc(sizeof(char)*strlen(cur_dir->entries[j].filename)+1);
                strcpy(word,cur_dir->entries[j].filename);
                array[counter] = word;
                //printf("LISTDIR: %s\n", array[counter]);
                counter++;
            }
        }
    }
    array[counter] = last;
    return array;
}

void file_printdir(char *path)
{
    char **array;
    char **ptr;

    array = file_listdir(path);
    ptr = array;

    while(strcmp(*ptr, "") != 0)
    {
        printf("%s\n", *ptr);
        ptr++;
    }
}

