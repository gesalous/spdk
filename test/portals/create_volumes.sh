#!/bin/bash

# Default inline data size (disabled)
INLINE_DATA_SIZE=0

# Usage instructions
usage() {
    echo "Usage: $0 <IP_ADDRESS> [num_volumes] [-inline] [rdma|tcp]"
    echo "Examples:"
    echo "  $0 192.168.1.100"
    echo "  $0 192.168.1.100 3"
    echo "  $0 192.168.1.100 3 -inline"
    echo "  $0 192.168.1.100 3 -inline tcp"
    exit 1
}

# Check if IP address parameter is provided
if [ -z "$1" ]; then
    usage
fi

IP_ADDRESS=$1
NUM_VOLUMES=1
TRANSPORT_TYPE="rdma"

# Parse arguments
if [ -n "$2" ]; then
    if [ "$2" == "-inline" ] || [ "$2" == "rdma" ] || [ "$2" == "tcp" ]; then
        NUM_VOLUMES=1
    else
        NUM_VOLUMES=$2
    fi
fi

if [ "$3" == "-inline" ]; then
    INLINE_DATA_SIZE=4096
fi

# Check for transport type in 3rd or 4th argument
if [ "$3" == "tcp" ] || [ "$3" == "rdma" ]; then
    TRANSPORT_TYPE="$3"
elif [ "$4" == "tcp" ] || [ "$4" == "rdma" ]; then
    TRANSPORT_TYPE="$4"
fi

echo "Creating $NUM_VOLUMES volumes on $IP_ADDRESS using $TRANSPORT_TYPE transport"
if [ $INLINE_DATA_SIZE -gt 0 ]; then
    echo "Inline data enabled (size: $INLINE_DATA_SIZE)"
fi

# Create multiple AIO bdevs
for i in $(seq 0 $((NUM_VOLUMES-1))); do
    echo "Creating AIO bdev: AIO$i"
    ./scripts/rpc.py bdev_aio_create /tmp/gesalous_junk"$i".dat AIO"$i" 512
done

# Create the transport
echo "Creating $TRANSPORT_TYPE transport"
if [ "$TRANSPORT_TYPE" == "rdma" ]; then
    ./scripts/rpc.py nvmf_create_transport -t RDMA \
        -u 8192 \
        -m 4096 \
        -c $INLINE_DATA_SIZE
elif [ "$TRANSPORT_TYPE" == "tcp" ]; then
    ./scripts/rpc.py nvmf_create_transport -t TCP \
        -o 8192 \
        -b 64
else
    echo "Unsupported transport type: $TRANSPORT_TYPE"
    exit 1
fi

# Create multiple NVMe-oF subsystems
for i in $(seq 1 "$NUM_VOLUMES"); do
    SUBSYSTEM_NAME="nqn.2016-06.io.spdk:cnode$i"
    SERIAL_NUMBER="SPDK$(printf "%011d" "$i")"
    CONTROLLER_NAME="SPDK_Controller$i"
    AIO_INDEX=$((i-1))  # AIO devices start from 0, cnode starts from 1

    echo "Creating subsystem: $SUBSYSTEM_NAME"

    # Create NVMe-oF subsystem
    ./scripts/rpc.py nvmf_create_subsystem "$SUBSYSTEM_NAME" \
        -a \
        -s "$SERIAL_NUMBER" \
        -d "$CONTROLLER_NAME"

    # Add namespace to subsystem
    ./scripts/rpc.py nvmf_subsystem_add_ns "$SUBSYSTEM_NAME" AIO"$AIO_INDEX"

    # Add listener to subsystem
    ./scripts/rpc.py nvmf_subsystem_add_listener "$SUBSYSTEM_NAME" \
        -t $TRANSPORT_TYPE \
        -a "$IP_ADDRESS" \
        -s 4420

    echo "Volume $i created successfully"
done

echo "All $NUM_VOLUMES volumes created successfully for $TRANSPORT_TYPE."
