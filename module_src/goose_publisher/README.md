Module to read data from /dev/shm, and publish them via GOOSE
Module can be run from the same process, or from a separate process

When running in the same process, the module should be called from event();

When running in a separate process, the module will poll index for each data-element for an update

