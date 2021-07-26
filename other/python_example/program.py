import zmq
import mem_file
import mmap
import struct
import time

# wait for zmq event

# then read data from file

# write data to file

if __name__ == "__main__":
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.bind("tcp://*:9001")
    socket.setsockopt(zmq.SUBSCRIBE, b"")

    file_p = open("/dev/shm/smv92.mem", "r+b",0)
    file = mmap.mmap(file_p.fileno(), 0)
    file.flush()
    file_p.flush()

    size = mem_file.read_size_file(file_p)
    items = mem_file.read_items_file(file_p)
    index = mem_file.read_index_file(file_p)
    type = mem_file.read_type_file(file_p)
    print("size=%i, items=%i, index=%i, type=%i" % (size,items, index, type))
    #file_p.close()

    while True:
        #  Wait for next request from client
        message = socket.recv()
        if message == b"smv_module1_SAMPLE_RECEIVED":
            print("mmap: %i" % int(struct.unpack('i', file[8:12])[0]) )
            print("index: %i" % mem_file.read_index_file(file_p) )
            time.sleep(0.000001)
