#!/bin/bash

# Default values
INLINE_DATA_SIZE=0
FILE_PREFIX="/tmp/gesalous_junk"

# Function to show usage
show_usage() {
    echo "Usage: $0 <IP_ADDRESS> [num_volumes] [-inline] [-prefix <file_prefix>]"
    echo "Examples:"
    echo "  $0 192.168.1.100"
    echo "  $0 192.168.1.100 3"
    echo "  $0 192.168.1.100 3 -inline"
    echo "  $0 192.168.1.100 3 -prefix /tmp/myfiles"
    echo "  $0 192.168.1.100 3 -inline -prefix /tmp/myfiles"
    echo ""
    echo "Options:"
    echo "  -inline           Enable inline data (4096 bytes)"
    echo "  -prefix <path>    Set file prefix (default: /tmp/gesalous_junk)"
}

# Check if IP address parameter is provided
if [ -z "$1" ]; then
    show_usage
    exit 1
fi

IP_ADDRESS=$1
NUM_VOLUMES=1

# Parse arguments
shift  # Remove IP address from arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -inline)
            INLINE_DATA_SIZE=4096
            shift
            ;;
        -prefix)
            if [ -z "$2" ]; then
                echo "Error: -prefix requires a value"
                show_usage
                exit 1
            fi
            FILE_PREFIX="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            # If it's a number, treat it as NUM_VOLUMES
            if [[ $1 =~ ^[0-9]+$ ]]; then
                NUM_VOLUMES=$1
            else
                echo "Error: Unknown option $1"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

echo "Creating $NUM_VOLUMES volumes on $IP_ADDRESS"
echo "File prefix: $FILE_PREFIX"
if [ $INLINE_DATA_SIZE -gt 0 ]; then
    echo "Inline data enabled (size: $INLINE_DATA_SIZE)"
fi

# Create multiple AIO bdevs
for i in $(seq 0 $((NUM_VOLUMES-1))); do
    FILE_PATH="${FILE_PREFIX}${i}.dat"
    echo "Creating AIO bdev: AIO$i (file: $FILE_PATH)"
    ./scripts/rpc.py bdev_aio_create "$FILE_PATH" AIO"$i" 4096
done

# Create the RDMA transport (only once)
echo "Creating RDMA transport"
./scripts/rpc.py nvmf_create_transport -t RDMA \
    -u 8192 \
    -m 4 \
    -c $INLINE_DATA_SIZE

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
        -t rdma \
        -a "$IP_ADDRESS" \
        -s 4420
        
    echo "Volume $i created successfully"
done

echo "All $NUM_VOLUMES volumes created successfully!"
echo "Files used:"
for i in $(seq 0 $((NUM_VOLUMES-1))); do
    echo "  ${FILE_PREFIX}${i}.dat"
done

