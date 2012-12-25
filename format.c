#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "api.h"
#include "filesystem.h"

int main(int argc, const char **argv)
{
    format_fs("disk.dat", 60480);
    return;
}

