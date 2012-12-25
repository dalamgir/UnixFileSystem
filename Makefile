CC = gcc
#CFLAGS =-g -ansi -pedantic -Wall -Wstrict-prototypes
CFLAGS =-g
#LDFLAGS=

all: driver format 

test_fs: test_fs.o filesystem.o
	${CC} -o test_fs test_fs.o filesystem.o

test_fs.o: test_fs.c api.h filesystem.h
	${CC} ${CFLAGS} -c test_fs.c  

driver: driver.o filesystem.o
	${CC} -o driver driver.o filesystem.o ${LDFLAGS}

format: format.o filesystem.o
	${CC} -o format format.o filesystem.o ${LDFLAGS}

format.o: format.c api.h filesystem.h
	${CC} ${CFLAGS} -c format.c

driver.o: driver.c api.h filesystem.h test_fs.c
	${CC} ${CFLAGS} -c driver.c

filesystem.o: api.h filesystem.h filesystem.c 
	${CC} ${CFLAGS} -c filesystem.c

handin:
	zip cs416_proj3.zip driver.c filesystem.c filesystem.h api.h Makefile driver.sh dump disk.dat format.c format.sh test_fs.c 

clean:
	rm -f driver
	rm -f driver.o
	rm -f filesystem.o
	rm -f format.o
	rm -f format
	rm -f test_fs
	rm -f test_fs.o
clean_disk:
	rm -f disk.dat
