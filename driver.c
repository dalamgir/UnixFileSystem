#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "api.h"
#include "filesystem.h"
#include "test_fs.c"

struct open_file_table_entry entries[20];

int main(int argc, const char **argv)
{

    int s, j, i1, i, num_files, fd, fd2;

    char s1[] = "/dir";
    char s2[12];

    i1 = 0;
    num_files = 0;

#define MY_FILE_SIZE 8
#define MY_FILE_SIZE2 8

    int array[MY_FILE_SIZE];
    int array2[MY_FILE_SIZE2];

    int bytes_written, bytes_read;
    int neq = 0;
    int seek_cur;
    int return_value, file_number;

    char **sarray;

    for(i = 0; i < MY_FILE_SIZE; i++)
    {
        array[i] = 5;
    }

    for(i = 0; i < MY_FILE_SIZE2 ; i++)
    {
        array2[i] = 6;
    }

    s = open_fs("disk.dat");

    if (s)
    {
        printf("Could not open filesystem.\n");
    }

    for(i = 0; i < num_files; i++)
    {
        sprintf(s2,"%s%d",s1, i);
        printf("%s \n", s2);
        file_create(s2);
    }

    test_fs();

    for(i = 0; i < num_files; i++)
    {
        sprintf(s2,"%s%d",s1, i);
        printf("%s \n", s2);
        file_delete(s2);
    }

    close_fs();
}

