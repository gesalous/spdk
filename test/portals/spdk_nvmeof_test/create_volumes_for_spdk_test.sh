sudo ./scripts/rpc.py nvmf_create_transport -t rdma -u 8192 -a 256
sudo ./scripts/rpc.py bdev_malloc_create -b Malloc0 64 512
sudo ./scripts/rpc.py nvmf_create_subsystem nqn.2016-06.io.spdk:testnqn -a -s SPDK00000000000001
sudo ./scripts/rpc.py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:testnqn Malloc0
sudo ./scripts/rpc.py nvmf_subsystem_add_listener nqn.2016-06.io.spdk:testnqn -t rdma -a 192.168.100.8 -s 4420
