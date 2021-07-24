#!/bin/bash

cd module_src/si/build
make
cd ../../smv_subscriber/build
make
cd ../../goose_publisher/build
make
cd ../../goose_subscriber/build
make
cd ../../smv_publisher/build
make
cd ../../ipc_signals/build
make
cd ../../mms_server/build
make
cd ../..
