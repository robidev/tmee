#ifndef mem_file_h__
#define mem_file_h__


char *mmap_file(char * filename, int size); // return char* to buffer
char *mmap_fd(int fd, int size);
int munmap_file(char * buffer, int size); // return char* to buffer
int calculate_buffer_size(int item_size, int max_items);
int calculate_buffer_size_from_file(int fd);
int type_to_item_size(int type);

int lock_buffer(volatile char * buffer, char spin);
int free_buffer_lock(char * buffer);

int read_size(char * buffer);
int read_items(char * buffer);
int read_index(char * buffer);
int read_type(char * buffer);
char read_input_bool(char * buffer, int index);
char read_input_int8(char * buffer, int index);
int read_input_int32(char * buffer, int index);
int write_type(char *buffer, short type);
int write_size(char *buffer, int size);
int write_items(char *buffer, int items);
int write_index(char *buffer, int index);
int write_output_int(char * buffer, int value);
int write_output_bool(char * buffer, char value);
int write_output_int8(char * buffer, char value);
int write_output_quality(char * buffer, int value);
int write_output_time(char * buffer, int value);
int write_output_float(char * buffer, float value);

#endif  // mem_file_h__