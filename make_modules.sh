#!/bin/bash

cd module_src/si/build
make
cd ../../smv_subscriber/build
make
cd ../../goose_publisher/build
make
