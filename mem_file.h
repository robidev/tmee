#ifndef mem_file_h__
#define mem_file_h__


inline char *mmap_file(char * filename, int size); // return char* to buffer
inline char *mmap_fd(int* fd, int size);
inline int munmap_file(char * buffer, int size); // return char* to buffer

inline int calculate_buffer_size(int item_size, int max_items);
inline int type_to_item_size(int type);

inline int lock_buffer(char * buffer, char spin);
inline int free_buffer_lock(char * buffer);


inline int read_size(char * buffer);
inline int read_items(char * buffer);
inline int read_index(char * buffer);
inline int read_type(char * buffer);

inline char read_input_bool(char * buffer, int index);
inline char read_input_int8(char * buffer, int index);
inline int read_input_int32(char * buffer, int index);


inline int write_type(char *buffer, short type);
inline int write_size(char *buffer, int size);
inline int write_items(char *buffer, int items);
inline int write_index(char *buffer, int index);

inline int write_output_int(char * buffer, int value);
int write_output_bool(char * buffer, char value);
int write_output_int8(char * buffer, char value);
int write_output_quality(char * buffer, int value);
int write_output_time(char * buffer, int value);
int write_output_float(char * buffer, float value);

#endif  // mem_file_h__