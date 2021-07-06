tmee (timed-module-execution-engine)

This program is a modular engine that allows several tasks to be run within a certain deadline, 
while monitoring overall performance and detect when deadlines are not being met.

Using this engine, modules can make data available to other modules in the same process, and signal when new data is available, creating a pipeline.
Dependencies can be configured in the config-file, and modules can be used mutiple times creating a flexible setup.
Additionaly, the module architecture allows processes to be run in separate processes as well, utilising parallel processing capabilities of the CPU.

The modules use /dev/shm/ for data-sharing, and preventing data-duplication
Within the same process, callbacks are chained to ensure minimal latency between data-processing sequences. 
Other processes detect new data using a busy-wait loop as a semaphore. 
(this might be extended in the future using other signaling, such as sockets, pipep or signals)

[drawing]