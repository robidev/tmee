here a python runtime example should be provided, that shows the following:
1. receiving events from zmq (when a trip occurs)
2. reading input files from tmpfs (read trip value)

3. processing data (example: increment trip counter on false->true)

4. writing output file to tmpfs (write trip-counter)
5. sending event to zmq (advertise update in trip-counter)
