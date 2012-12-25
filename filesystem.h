#include<stdio.h>

#define ERR_INTERNAL -20
#define ERR_MIN_BLOCKS -21
#define ERR_INVALID_LSEEK_CMD -22
#define ERR_INVALID_LSEEK_OFFSET -23
#define ERR_FILE_ALREADY_OPEN -24
#define ERR_NOT_A_FILE -25
#define ERR_NOT_A_DIR -26
#define ERR_INVALID_DISK_FILE -27

#define BLOCK_SIZE 512
#define MAX_OPEN_FILES 20

typedef unsigned char BYTE;
typedef unsigned int BLOCK;

// 512 Bytes
struct bootblock
{
    BLOCK padding[128];
};

struct datablock
{
    BYTE byte[512];
};

// 512 Bytes
struct superblock
{
    int fs_type;
    int disk_size;
    int blocks_allocated;
    int max_blocks;
    int files_allocated;
    int max_files;
    int free_inode_list;

    BLOCK free_data_block_list;

    BYTE padding[480];
};

// 512 Bytes
struct free_data_block
{
    BLOCK next_free_block;
    BYTE pad[508];
};

// 64 Bytes
struct inode
{
    int next_free_inode;
    int is_free;
    int num_blocks;
    int is_dir;
    BLOCK file_blocks[10];
    BLOCK indirect1;
    BLOCK indirect2;
};

// 512 Bytes
struct inode_block
{
    struct inode inodes[8];
};

struct directory_entry
{
    char filename[12];
    int inode_number;
};

// 512 Bytes
struct directory
{
    struct directory_entry entries[32];
};

// 512 bytes
struct indirection_block
{
    BLOCK pointer[128];
};

/* This is the open_file_table structure..It contains more information about byte offsets and stuff like that
Im not sure if we're supposed to take that stuff into account..right now im leaving this structure for testing
purposes.
*/
struct open_file_table_entry
{
    int inode_number;
    int seek_position;
    int currently_opened;
};


//GLOBALS
int NUM_BLOCKS;
int NUM_INODE_BLOCKS;
int NUM_INODES;
int NUM_DATA_BLOCKS;
int NUM_INODES_PER_BLOCK;

// Global int that holds the file index for disk.dat
int file;
int pwd; /* current working directory...probably could be local instead... */
struct open_file_table_entry open_file_table[MAX_OPEN_FILES];

//Helper Functions
int write_block(int file, const void *buf, int block_num);
int read_block(int file, void *buf, int block_num);

//give an inode number return inode block containing that inode
struct inode_block *get_inode_block(int inode_num);

//give an inode number and inode block, will write back to correct place
int put_inode_block(struct inode_block *ib, int inode_num);

//give inode number, ref to inode_block and ref to inode, reads into inode_block and inode, returns error codes
int get_inode(struct inode_block **inode_block, struct inode **inode, int inode_num);

//returns a free inode number, this function handles updating the superblock and removing inode from free list
int get_free_inode(void);

//returns a free datablock number, this function handles updating the superblock removing from free db list
int get_free_datablock(void);

//adds a datablock to an inode (NEEDS MORE TESTING FOR LARGE FILES)
int add_data_block(int inode_num);

//attempts to add a new directory(or file) to an inode_num
int add_dir_to_inode(int inode_num, char *n_dir, int n_inode_num);

//reads inode's file_block_num into dblk and returns the data block number for easy write back
int get_data_block(struct datablock **dblk, struct inode *inode, int file_block_num);

//returns -2 on errors, -1 if file not found, inode number >=0 if has file
int has_file(struct inode *cur_inode, char *cur);

//enter a path and 1 to create a directory, enter a path and 0 to creat a file, returns error codes
int create_file(char *path, int is_dir);

//given a path returns inode number or error codes if not found
int path_to_inode(char *path);

//updates superblock and writes a freedatablock to db_num
int make_free_datablock(int db_num);

//remove all datablocks and indirection blocks associated w file
//blank out inode, set it free and update superblock
int erase_inode(int inode_num);

//run this everytime we reduce numblocks of an inode, frees an indirection
//blocks that are no longer needed.
int trim_indirection_blocks(int inode_num);

//this function deletes dirs or files, will wrap this for api
int delete_file(char *path);

//this is the hack_funct...not being used atm
int hack_funct();

