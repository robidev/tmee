Module to read goose packets, and publish them on /dev/shm
Module can be run from the same process, or from a separate process

When running in a separate process, the module will increment the sample-counter index in the output-file

When running in the same process, the module will trigger a callback when a packet is received

Using the callback-mechanism, additional signaling may be used as well, such as sockets, signals or pipes by using
a separate module
