#!/bin/bash

# Create the AIO bdev
./scripts/rpc.py bdev_aio_create /tmp/gesalous_junk.dat AIO0 4096

# Create the RDMA transport
./scripts/rpc.py nvmf_create_transport -t RDMA \
  -u 8192 \
  -m 4 \
  -c 8192

# Create NVMe-oF subsystem
./scripts/rpc.py nvmf_create_subsystem nqn.2016-06.io.spdk:cnode1 \
  -a \
  -s SPDK00000000000001 \
  -d SPDK_Controller

# Add namespace to subsystem
./scripts/rpc.py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 AIO0

# Add listener to subsystem
./scripts/rpc.py nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 \
  -t rdma \
  -a 192.168.5.122 \
  -s 4420

