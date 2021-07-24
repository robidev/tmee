import struct
import sys

#https://docs.python.org/3/library/struct.html

# constants
SIZE_POS=0
ITEMS_POS=4
INDEX_POS=8
TYPE_POS=12
SEMAPHORE_POS=14
DATA_POS=16

# types
BOOL  =      1  # item size = 1
INT8  =      2  # item size = 1
INT32 =      3  # item size = 4
INT64 =      4  # item size = 8
FLOAT32 =    5  # item size = 4
FLOAT64 =    6  # item size = 8
QUALITY =    7  # item size = 4
MMSSTRING =  8  # item size = max strlen
BITSTRING =  9  # item size is then in bits instead of bytes
VISSTRING =  10 # item size = max strlen
OCTSTRING =  11 # item size = max strlen
GENTIME =    12 # item size = 4?
BINTIME =    13 # item size = 4?
UTCTIME =    14 # item size = 4?
GOOSE =      20 # items size 1024...
SMV92 =      68 # item size = 64

# item sizes
def type_to_item_size(type):
    switch = {
        "BOOL":1,
        "INT8":1,
        "INT32":4,
        "INT64":8,
        "FLOAT32":4,
        "FLOAT64":8,
        "QUALITY":4,
        "SMV92":68,
        "GOOSE":1024,
    }
    return switch.get(type,"unknown")


def read_size(file):
    file.seek(SIZE_POS, 0)
    return int(struct.unpack('i', file.read(4))[0])


def read_items(file):
    file.seek(ITEMS_POS, 0)
    return int(struct.unpack('i', file.read(4))[0])


def read_index(file):
    file.seek(INDEX_POS, 0)
    return int(struct.unpack('i', file.read(4))[0])


def read_type(file):
    file.seek(TYPE_POS, 0)
    return int(struct.unpack('h', file.read(2))[0])


def lock_buffer(file, spin):
    old = file.tell()
    file.seek(SEMAPHORE_POS, 0)
    if spin == False:
        if file.read(1) == 1:
            file.seek(old,0)
            return -1
    else:
        while file.read(1) == 1: #busy wait loop
            file.seek(SEMAPHORE_POS, 0)
            
    file.seek(SEMAPHORE_POS, 0)
    file.write( bytes(1) )
    file.seek(old,0)
    return 0


def free_buffer_lock(file):
    old = file.tell()
    file.seek(SEMAPHORE_POS, 0)
    if file.read(1) == 0:
        file.seek(old,0)
        return -1

    file.seek(SEMAPHORE_POS, 0)
    file.write( bytes(0) )
    file.seek(old,0)
    return 0


def read_input_bool(file, index):
    file.seek(DATA_POS + index, 0)
    lock_buffer(file, True)
    result = bool(file.read(1))
    free_buffer_lock(file)
    return result

def read_input_int8(file, index):
    file.seek(DATA_POS + index, 0)
    lock_buffer(file, True)
    result = int(file.read(1))
    free_buffer_lock(file)
    return result

def read_input_int32(file, index):
    file.seek(DATA_POS + (index * 4), 0)
    lock_buffer(file, True)
    result = int(struct.unpack('i', file.read(4)))
    free_buffer_lock(file)
    return result

def read_input_smv(file, var_index, index):
    file.seek(DATA_POS + (index * 68) + var_index, 0)
    lock_buffer(file, True)
    result = int(struct.unpack('i', file.read(4)))
    free_buffer_lock(file)
    return result


def write_type(file, type):
    file.seek(TYPE_POS, 0)
    lock_buffer(file, True)
    file.write( bytes(struct.pack("i", type)) )
    free_buffer_lock(file)


def write_size(file, size):
    file.seek(SIZE_POS, 0)
    lock_buffer(file, True)
    file.write( bytes(struct.pack("i", size)) )
    free_buffer_lock(file)


def write_items(file, items):
    file.seek(ITEMS_POS, 0)
    lock_buffer(file, True)
    file.write( bytes(struct.pack("i", items)) )
    free_buffer_lock(file)


def write_index(file, index):
    file.seek(INDEX_POS, 0)
    lock_buffer(file, True)
    file.write( bytes(struct.pack("i", index)) )
    free_buffer_lock(file)


def write_output_int(file, value):
    if read_type(file) != INT32 or read_size(file) != 4:
        return -1

    items = read_items(file);

    lock_buffer(file,1);

    index = (read_index(file) + 1) % items;

    file.seek(DATA_POS + (index * 4), 0)
    file.write( bytes(struct.pack("i", value)) )

    write_index(file,index)

    free_buffer_lock(file);


def write_output_bool(file, value):
    if read_type(file) != BOOL or read_size(file) != 1:
        return -1

    items = read_items(file);

    lock_buffer(file,1);

    index = (read_index(file) + 1) % items;

    file.seek(DATA_POS + (index), 0)
    file.write( bytes(struct.pack("?", value)) )

    write_index(file,index)

    free_buffer_lock(file);


def write_output_int8(file, value):
    if read_type(file) != INT8 or read_size(file) != 1:
        return -1

    items = read_items(file);

    lock_buffer(file,1);

    index = (read_index(file) + 1) % items;

    file.seek(DATA_POS + (index), 0)
    file.write( bytes(struct.pack("b", value)) )

    write_index(file,index)

    free_buffer_lock(file);



def write_output_quality(file, value):
    if read_type(file) != QUALITY or read_size(file) != 4:
        return -1

    items = read_items(file);

    lock_buffer(file,1);

    index = (read_index(file) + 1) % items;

    file.seek(DATA_POS + (index * 4), 0)
    file.write( bytes(struct.pack("i", value)) )

    write_index(file,index)

    free_buffer_lock(file);


def write_output_time(file, value):
    if read_type(file) != GENTIME or read_size(file) != 4:
        return -1

    items = read_items(file);

    lock_buffer(file,1);

    index = (read_index(file) + 1) % items;

    file.seek(DATA_POS + (index * 4), 0)
    file.write( bytes(struct.pack("i", value)) )

    write_index(file,index)

    free_buffer_lock(file);


def write_output_float(file, value):
    if read_type(file) != FLOAT32 or read_size(file) != 4:
        return -1

    items = read_items(file);

    lock_buffer(file,1);

    index = (read_index(file) + 1) % items;

    file.seek(DATA_POS + (index * 4), 0)
    file.write( bytes(struct.pack("f", value)) )

    write_index(file,index)

    free_buffer_lock(file);


def read_current_input_bool(file):
    index = read_index(file)
    return read_input_bool(file, index)


def read_current_input_int8(file):
    index = read_index(file)
    return read_input_int8(file, index)


def read_current_input_int32(file):
    index = read_index(file)
    return read_input_int32(file, index)


#testing
file = open("/dev/shm/smv92.mem", "r+b")
size = read_size(file)
items = read_items(file)
index = read_index(file)
type = read_type(file)
print("size=%i, items=%i, index=%i, type=%i" % (size,items, index, type))

result = lock_buffer(file,True)
print("lock result= %i" % result)
result = free_buffer_lock(file)
print("unlock result= %i" % result)

print("done")


