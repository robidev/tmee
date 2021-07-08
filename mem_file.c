#include "types.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

//index in int(4 byte size)
#define ITEM_SIZE   0
#define ITEM_ITEMS  1
#define ITEM_INDEX  2
#define ITEM_TYPE   3

//index in byte(1 byte size)
#define SEMAPHORE_POS   14

inline char *mmap_file(char * filename, int size) // return char* to buffer
{
    int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );//shm_open(STORAGE_ID, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        printf("open issue for %s:\n", filename);
        perror("open");
        return 0;
    }
    
    char *buffer = mmap_fd(fd, size);
    close(fd);
    return buffer;
}

inline char *mmap_fd(int* fd, int size) // return char* to buffer
{ 
    char *buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED)
    {
        printf("mmap issue:\n");
        perror("mmap");
        return 0;
    }
    return buffer;
}

inline int munmap_file(char * buffer, int size) // return char* to buffer
{
    return munmap(buffer, size);
}

inline int calculate_buffer_size(int item_size, int max_items)
{
    return 16 + item_size*max_items;
}

inline int type_to_item_size(int type)
{
    switch(type)
    {
        case 0:
            return 0;
        case BOOL:
            return 1;
        case INT8:
            return 1;
        case INT32:
            return 4;
        case INT64:
            return 8;
        case FLOAT32:
            return 4;
        case FLOAT64:
            return 8;
        case QUALITY:
            return 4;
        case SMV92:
            return 64;
        default:
            return 0;
    }
}

inline int read_size(char * buffer)
{
    int *tmp = (int *)buffer;
    return tmp[0];
}

inline int read_items(char * buffer)
{
    int *tmp = (int *)buffer;
    return tmp[1];
}

inline int read_index(char * buffer)
{
    int *tmp = (int *)buffer;
    return tmp[2];
}

inline int read_type(char * buffer)
{
    int *tmp = (int *)buffer;
    return tmp[3] & 0x0000FFFF;
}


inline int lock_buffer(char * buffer, char spin)
{
    if (spin == 0)//test or wait
    {
        if(buffer[SEMAPHORE_POS])
        {
            return -1;
        }
    }
    else
    {
        while(buffer[SEMAPHORE_POS]); //busy wait
    }
    buffer[SEMAPHORE_POS] = 1; //write semaphore to 1
    return 0;   
}

inline int free_buffer_lock(char * buffer)
{
    if(!buffer[SEMAPHORE_POS])//if already 0
    {
        return -1;
    }
    buffer[SEMAPHORE_POS] = 0; //write semaphore to 0
    return 0; 
}

inline char read_input_bool(char * buffer, int index)
{
    return buffer[index];
}

inline char read_input_int8(char * buffer, int index)
{
    return buffer[index];
}

inline int read_input_int32(char * buffer, int index)
{
    int *tmp = (int *)buffer;
    return tmp[index];
}

inline int write_type(char *buffer, short type)
{
    lock_buffer(buffer,1);
    short *tmp = (short *)buffer;
    tmp[6] = type;
    free_buffer_lock(buffer);
}

inline int write_size(char *buffer, int size)
{
    lock_buffer(buffer,1);
    int *tmp = (int *)buffer;
    tmp[0] = size;
    free_buffer_lock(buffer);
}

inline int write_items(char *buffer, int items)
{
    lock_buffer(buffer,1);
    int *tmp = (int *)buffer;
    tmp[1] = items;
    free_buffer_lock(buffer);
}

inline int write_index(char *buffer, int index)
{
    lock_buffer(buffer,1);
    int *tmp = (int *)buffer;
    tmp[2] = index;
    free_buffer_lock(buffer);
}

inline int write_output_int(char * buffer, int value)
{
    int *tmp = (int *)buffer;
    if(read_type(buffer) != INT32 || read_size(buffer) != 4) return -1;
    int items = read_items(buffer);

    lock_buffer(buffer,1);

    int index = (read_index(buffer) + 1) % items;
    tmp[ 4 + index] = value;
    tmp[2] = index; //increment index

    free_buffer_lock(buffer);
}

int write_output_bool(char * buffer, char value)
{
    int *tmp = (int *)buffer;
    if(read_type(buffer) != INT32 || read_size(buffer) != 4) return -1;
    int items = read_items(buffer);

    lock_buffer(buffer,1);

    int index = (read_index(buffer) + 1) % items;
    buffer[ 16 + index] = value;
    tmp[2] = index; //increment index

    free_buffer_lock(buffer);
}

int write_output_int8(char * buffer, char value)
{
    int *tmp = (int *)buffer;
    if(read_type(buffer) != INT32 || read_size(buffer) != 4) return -1;
    int items = read_items(buffer);

    lock_buffer(buffer,1);

    int index = (read_index(buffer) + 1) % items;
    buffer[ 16 + index] = value;
    tmp[2] = index; //increment index

    free_buffer_lock(buffer);
}

int write_output_quality(char * buffer, int value)
{
    int *tmp = (int *)buffer;
    if(read_type(buffer) != QUALITY || read_size(buffer) != 4) return -1;
    int items = read_items(buffer);

    lock_buffer(buffer,1);

    int index = (read_index(buffer) + 1) % items;
    tmp[ 4 + index] = value;
    tmp[2] = index; //increment index

    free_buffer_lock(buffer);
}

int write_output_time(char * buffer, int value)
{
    int *tmp = (int *)buffer;
    if(read_type(buffer) != GENTIME || read_size(buffer) != 4) return -1;
    int items = read_items(buffer);

    lock_buffer(buffer,1);

    int index = (read_index(buffer) + 1) % items;
    tmp[ 4 + index] = value;
    tmp[2] = index; //increment index

    free_buffer_lock(buffer);
}

int write_output_float(char * buffer, float value)
{
    float *tmp = (float *)buffer;
    if(read_type(buffer) != FLOAT32 || read_size(buffer) != 4) return -1;
    int items = read_items(buffer);

    lock_buffer(buffer,1);

    int index = (read_index(buffer) + 1) % items;
    tmp[ 4 + index] = value;
    tmp[2] = index; //increment index

    free_buffer_lock(buffer);
}

