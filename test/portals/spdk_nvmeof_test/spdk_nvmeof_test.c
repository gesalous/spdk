#include "../../../lib/rdma_provider/ptl_log.h"
#include <getopt.h>
#include <pthread.h>
#include <spdk/env.h>
#include <spdk/log.h>
#include <spdk/nvme.h>
#include <spdk/string.h>
#include <sys/time.h>

#define MAX_VOLUMES 16
#define MAX_IO_QUEUES 32
#define DEFAULT_IO_SIZE 4096
#define DEFAULT_NUM_IO_QUEUES 4
#define DEFAULT_QUEUE_SIZE 1
#define DEFAULT_NUM_THREADS 1

struct volume_context {
	struct spdk_nvme_ctrlr *ctrlr;
	struct spdk_nvme_ns *ns;
	struct spdk_nvme_qpair *qpair[MAX_IO_QUEUES];
	uint64_t size;
	uint64_t total_lbas;
	uint32_t nsid;
	char *transport_id;
	bool connected;
	int volume_id;
};

struct test_context {
	struct volume_context volumes[MAX_VOLUMES];
	int num_volumes;
	int completed_ios;
	int total_ios;
	uint32_t io_size;
	bool is_write;
	int ios_per_volume;
	int num_io_queues;
	int queue_size;
	int num_threads;
};

static struct test_context g_test_ctx = {0};


struct io_thread {
	struct test_context *g_test_ctx;
	volatile int ios_submitted;
	volatile int ios_completed;
	int worker_id;
	int volume_id;
	int io_queue_id;
	int queue_size;
	bool is_write;
};


static void io_complete_cb(void *arg, const struct spdk_nvme_cpl *completion)
{
	struct io_thread *io_thread = arg;
	struct volume_context *vol_ctx = &io_thread->g_test_ctx->volumes[io_thread->volume_id];
	if (spdk_nvme_cpl_is_error(completion)) {
		SPDK_PTL_FATAL("Volume %d: I/O error - status: %s ios_submitted: %d ios_completed: %d",
			       vol_ctx->volume_id,
			       spdk_nvme_cpl_get_status_string(&completion->status), io_thread->ios_submitted,
			       io_thread->ios_completed);
	}
	io_thread->ios_completed++;
}

static bool
probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	 struct spdk_nvme_ctrlr_opts *opts)
{
	SPDK_PTL_INFO("Probing controller: %s max I/O queues supported are: %d timeout (default) is %u\n",
		      trid->traddr,
		      opts->num_io_queues, opts->keep_alive_timeout_ms);
	opts->keep_alive_timeout_ms = 90000;
	return true;
}

static void
attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
	  struct spdk_nvme_ctrlr *ctrlr,
	  const struct spdk_nvme_ctrlr_opts *opts)
{
	struct volume_context *vol_ctx = (struct volume_context *)cb_ctx;
	struct spdk_nvme_ns *ns;
	uint32_t nsid;

	SPDK_PTL_INFO("Volume %d: Controller attached: %s", vol_ctx->volume_id, trid->traddr);

	vol_ctx->ctrlr = ctrlr;

	// Find the first active namespace
	for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr);
	     nsid != 0;
	     nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid)) {

		ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
		if (ns == NULL) {
			continue;
		}

		if (!spdk_nvme_ns_is_active(ns)) {
			continue;
		}

		vol_ctx->ns = ns;
		vol_ctx->nsid = nsid;
		vol_ctx->size = spdk_nvme_ns_get_size(ns);
		vol_ctx->total_lbas = vol_ctx->size / spdk_nvme_ns_get_sector_size(vol_ctx->ns);

		SPDK_PTL_INFO("Volume no %d of size: %lu Found active namespace %u\n", vol_ctx->volume_id,
			      vol_ctx->size, nsid);
		break;
	}

	if (vol_ctx->ns == NULL) {
		SPDK_PTL_INFO("Volume %d: No active namespace found", vol_ctx->volume_id);
		return;
	}

	// Create I/O queue pairs
	for (int i = 0; i < g_test_ctx.num_io_queues; i++) {
		vol_ctx->qpair[i] = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
		if (vol_ctx->qpair[i] == NULL) {
			SPDK_PTL_INFO("Volume %d: Failed to allocate I/O queue pair %d", vol_ctx->volume_id, i);
			return;
		}
		SPDK_PTL_INFO("Volume %d: Created I/O queue pair %d", vol_ctx->volume_id, i);
	}

	vol_ctx->connected = true;
}

static int
connect_to_volume(int volume_id, const char *transport_addr, const char *transport_svcid,
		  const char *subnqn)
{
	struct volume_context *vol_ctx = &g_test_ctx.volumes[volume_id];
	struct spdk_nvme_transport_id trid = {};
	int rc;

	vol_ctx->volume_id = volume_id;

	// Setup transport ID
	snprintf(trid.traddr, sizeof(trid.traddr), "%s", transport_addr);
	snprintf(trid.trsvcid, sizeof(trid.trsvcid), "%s", transport_svcid);
	snprintf(trid.subnqn, sizeof(trid.subnqn), "%s", subnqn);
	trid.trtype = SPDK_NVME_TRANSPORT_RDMA;
	trid.adrfam = SPDK_NVMF_ADRFAM_IPV4;

	SPDK_PTL_INFO("Volume %d: Connecting to %s:%s (subnqn: %s)",
		      volume_id, transport_addr, transport_svcid, subnqn);

	// Probe and attach
	rc = spdk_nvme_probe(&trid, vol_ctx, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		SPDK_PTL_FATAL("Volume %d: Failed to probe NVMe controller", volume_id);
		return rc;
	}

	return 0;
}


static void *io_worker(void *args)
{
	struct io_thread *io_thread = args;
	struct volume_context *vol_ctx = &g_test_ctx.volumes[io_thread->volume_id];
	uint32_t lba_size, num_lbas;
	int rc;

	if (!vol_ctx->connected || vol_ctx->ns == NULL) {
		SPDK_PTL_FATAL("Volume %d: not connected", io_thread->volume_id);
	}
	SPDK_PTL_INFO("Worker[%d] volume id: %d I/O queue id: %d", io_thread->worker_id,
		      io_thread->volume_id, io_thread->io_queue_id);

	io_thread->ios_submitted = 0;
	io_thread->ios_completed = 0;

	// Use the queue_size from global context
	io_thread->queue_size = g_test_ctx.queue_size;

	// Allocate queue_size buffers
	void **buffers = calloc(io_thread->queue_size, sizeof(void *));
	if (!buffers) {
		SPDK_PTL_FATAL("Failed to allocate buffer array");
	}
	for (int i = 0; i < io_thread->queue_size; i++) {
		buffers[i] = spdk_dma_zmalloc(g_test_ctx.io_size, 0x1000, NULL);
		if (!buffers[i]) {
			SPDK_PTL_FATAL("Failed to allocate DMA buffer %d", i);
		}
		memset(buffers[i], 0xAA, g_test_ctx.io_size);
	}

	lba_size = spdk_nvme_ns_get_sector_size(vol_ctx->ns);
	num_lbas = g_test_ctx.io_size / lba_size;

	while (io_thread->ios_submitted < io_thread->g_test_ctx->ios_per_volume) {
		// Submit up to queue_size outstanding I/Os
		while ((io_thread->ios_submitted - io_thread->ios_completed) < io_thread->queue_size &&
		       io_thread->ios_submitted < g_test_ctx.ios_per_volume) {

			uint32_t io_idx = io_thread->ios_submitted;
			uint64_t lba_start = ((uint64_t)io_idx * num_lbas) % (vol_ctx->total_lbas - num_lbas + 1);
			void *buf = buffers[io_idx % io_thread->queue_size];

			rc = io_thread->is_write ?
			     spdk_nvme_ns_cmd_write(
				     vol_ctx->ns,
				     vol_ctx->qpair[io_thread->io_queue_id],
				     buf, lba_start, num_lbas,
				     io_complete_cb, io_thread, 0) :
			     spdk_nvme_ns_cmd_read(
				     vol_ctx->ns,
				     vol_ctx->qpair[io_thread->io_queue_id],
				     buf, lba_start, num_lbas,
				     io_complete_cb, io_thread, 0);


			if (rc) {
				SPDK_PTL_FATAL("Worker[%d] failed to submit I/O %u (rc=%d)\n",
					       io_thread->worker_id, io_idx, rc);
			}
			io_thread->ios_submitted++;
		}

		// Wait for at least one completion
		while (io_thread->ios_submitted - io_thread->ios_completed >= io_thread->queue_size) {
			SPDK_PTL_DEBUG("Queue overflow size: %d submitted: %d completed: %d",
				       io_thread->ios_submitted - io_thread->ios_completed, io_thread->ios_submitted,
				       io_thread->ios_completed);
			spdk_nvme_qpair_process_completions(
				io_thread->g_test_ctx->volumes[io_thread->volume_id].qpair[io_thread->io_queue_id], 0);
			spdk_nvme_ctrlr_process_admin_completions(vol_ctx->ctrlr);
		}
	}

	//Check remaining if any
	while (io_thread->ios_submitted != io_thread->ios_completed) {
		spdk_nvme_qpair_process_completions(
			io_thread->g_test_ctx->volumes[io_thread->volume_id].qpair[io_thread->io_queue_id], 0);
	}

	for (int i = 0; i < io_thread->queue_size; i++) {
		spdk_dma_free(buffers[i]);
	}
	free(buffers);
	return NULL;
}

static void cleanup_volumes(void)
{
	SPDK_PTL_INFO("\n=== Cleaning up ===\n");

	for (int i = 0; i < g_test_ctx.num_volumes; i++) {
		struct volume_context *vol_ctx = &g_test_ctx.volumes[i];

		if (!vol_ctx->connected) {
			continue;
		}

		// Free I/O queue pairs
		for (int j = 0; j < g_test_ctx.num_io_queues; j++) {
			if (vol_ctx->qpair[j] != NULL) {
				spdk_nvme_ctrlr_free_io_qpair(vol_ctx->qpair[j]);
				SPDK_PTL_INFO("Volume %d: Freed I/O queue pair %d\n", i, j);
			}
		}

		// Detach controller
		if (vol_ctx->ctrlr != NULL) {
			spdk_nvme_detach(vol_ctx->ctrlr);
			SPDK_PTL_INFO("Volume %d: Controller detached\n", i);
		}
	}
}

static void
print_usage(const char *program_name)
{
	printf("\nUsage: %s [OPTIONS] <num_volumes>\n", program_name);
	printf("Options:\n");
	printf("  -s, --io-size SIZE    I/O size in bytes (512 or multiple of 4KB, default: %d)\n",
	       DEFAULT_IO_SIZE);
	printf("  -o, --operation OP    Operation type: 'read' or 'write' (default: read)\n");
	printf("  -n, --num-ios COUNT    Number of I/O operations per volume (default: 1)\n");
	printf("  -q, --num-queues Q    Number of I/O queues per volume (default: %d, max: %d)\n",
	       DEFAULT_NUM_IO_QUEUES, MAX_IO_QUEUES);
	printf("  -Q, --queue-size N    Queue size per thread (default: %d)\n", DEFAULT_QUEUE_SIZE);
	printf("  -T, --num-threads N   Number of threads (default: %d)\n", DEFAULT_NUM_THREADS);
	printf("  -a, --addr ADDRESS    Target transport address (default: 192.168.1.100)\n");
	printf("  -p, --port PORT    Target transport port (default: 4420)\n");
	printf("  -h, --help    Show this help message\n");
	printf("\nExamples:\n");
	printf("  %s -s 8192 -o write -n 10 -q 8 -Q 4 -T 2 3\n", program_name);
	printf("  %s --io-size 4096 --operation read --num-ios 5 --num-queues 2 --queue-size 8 --num-threads 4 2\n",
	       program_name);
}

static int
validate_io_size(uint32_t io_size)
{
	if (io_size == 512) {
		return 0;
	}

	if (io_size < 4096 || (io_size % 4096) != 0) {
		return -1;
	}

	return 0;
}

int
main(int argc, char **argv)
{

	struct spdk_env_opts opts;
	int rc;
	int num_volumes;
	const char *transport_addr = "192.168.1.100";
	const char *transport_svcid = "4420";

	// Default values
	g_test_ctx.io_size = DEFAULT_IO_SIZE;
	g_test_ctx.is_write = false;
	g_test_ctx.ios_per_volume = 1;
	g_test_ctx.num_io_queues = DEFAULT_NUM_IO_QUEUES;
	g_test_ctx.queue_size = DEFAULT_QUEUE_SIZE;
	g_test_ctx.num_threads = DEFAULT_NUM_THREADS;

	// Command line options
	static struct option long_options[] = {
		{"io-size",    required_argument, 0, 's'},
		{"operation",  required_argument, 0, 'o'},
		{"num-ios",    required_argument, 0, 'n'},
		{"num-queues", required_argument, 0, 'q'},
		{"queue-size", required_argument, 0, 'Q'},
		{"num-threads", required_argument, 0, 'T'},
		{"addr",    required_argument, 0, 'a'},
		{"port",    required_argument, 0, 'p'},
		{"help",    no_argument,    0, 'h'},
		{0, 0, 0, 0}
	};

	int option_index = 0;
	int c;

	while ((c = getopt_long(argc, argv, "s:o:n:q:Q:T:a:p:h", long_options, &option_index)) != -1) {
		switch (c) {
		case 's':
			g_test_ctx.io_size = atoi(optarg);
			if (validate_io_size(g_test_ctx.io_size) != 0) {
				SPDK_PTL_WARN("Invalid I/O size: %u. Must be 512 or multiple of 4KB\n",
					      g_test_ctx.io_size);
				return 1;
			}
			break;
		case 'o':
			if (strcmp(optarg, "read") == 0) {
				g_test_ctx.is_write = false;
			} else if (strcmp(optarg, "write") == 0) {
				g_test_ctx.is_write = true;
			} else {
				SPDK_PTL_WARN("Invalid operation: %s. Must be 'read' or 'write'\n", optarg);
				return 1;
			}
			break;
		case 'n':
			g_test_ctx.ios_per_volume = atoi(optarg);
			if (g_test_ctx.ios_per_volume <= 0) {
				SPDK_PTL_WARN("Invalid number of I/Os: %d. Must be positive\n",
					      g_test_ctx.ios_per_volume);
				return 1;
			}
			break;
		case 'q':
			g_test_ctx.num_io_queues = atoi(optarg);
			if (g_test_ctx.num_io_queues <= 0 || g_test_ctx.num_io_queues > MAX_IO_QUEUES) {
				SPDK_PTL_WARN("Invalid number of I/O queues: %d. Must be between 1 and %d\n",
					      g_test_ctx.num_io_queues, MAX_IO_QUEUES);
				return 1;
			}
			break;
		case 'Q':
			g_test_ctx.queue_size = atoi(optarg);
			if (g_test_ctx.queue_size <= 0) {
				SPDK_PTL_WARN("Invalid queue size: %d. Must be positive\n",
					      g_test_ctx.queue_size);
				return 1;
			}
			break;
		case 'T':
			g_test_ctx.num_threads = atoi(optarg);
			if (g_test_ctx.num_threads <= 0) {
				SPDK_PTL_WARN("Invalid number of threads: %d. Must be positive\n",
					      g_test_ctx.num_threads);
				return 1;
			}
			break;
		case 'a':
			transport_addr = optarg;
			break;
		case 'p':
			transport_svcid = optarg;
			break;
		case 'h':
			print_usage(argv[0]);
			return 0;
		case '?':
			print_usage(argv[0]);
			return 1;
		default:
			break;
		}
	}

	// Check if num_volumes is provided
	if (optind >= argc) {
		SPDK_PTL_WARN("Missing required argument: num_volumes\n");
		print_usage(argv[0]);
		return 1;
	}

	num_volumes = atoi(argv[optind]);

	if (num_volumes <= 0 || num_volumes > MAX_VOLUMES) {
		SPDK_PTL_WARN("Number of volumes must be between 1 and %d\n", MAX_VOLUMES);
		return 1;
	}

	g_test_ctx.num_volumes = num_volumes;

	SPDK_PTL_INFO("\n=== SPDK NVMe-oF Multi-Volume Test ===\n"
		      "Volumes to connect: %d\n"
		      "Target address: %s:%s\n"
		      "I/O queues per volume: %d\n"
		      "Queue size: %d\n"
		      "Number of threads: %d\n"
		      "I/O size: %u bytes\n"
		      "Operation: %s\n"
		      "I/Os per volume: %d\n",
		      num_volumes, transport_addr, transport_svcid, g_test_ctx.num_io_queues,
		      g_test_ctx.queue_size, g_test_ctx.num_threads,
		      g_test_ctx.io_size, g_test_ctx.is_write ? "write" : "read",
		      g_test_ctx.ios_per_volume);

	// Initialize SPDK environment
	spdk_env_opts_init(&opts);
	opts.name = "nvmeof_test";

	if (spdk_env_init(&opts) < 0) {
		SPDK_PTL_INFO("Failed to initialize SPDK environment\n");
		return 1;
	}

	SPDK_PTL_INFO("\n=== Connecting to Volumes ===\n");

	// Connect to multiple volumes (assuming different subsystems)
	for (int i = 0; i < num_volumes; i++) {
		char subnqn[256];
		snprintf(subnqn, sizeof(subnqn), "nqn.2016-06.io.spdk:cnode%d", i + 1);

		rc = connect_to_volume(i, transport_addr, transport_svcid, subnqn);
		if (rc != 0) {
			SPDK_PTL_FATAL("Failed to connect to volume %d\n", i);
		}

		// Small delay between connections
		usleep(10000); // 100ms
	}

	// Wait a bit for all connections to establish
	sleep(2);
	int io_queue_id = 0;
	int volume_id = 0;
	pthread_t *worker_handle = calloc(g_test_ctx.num_threads, sizeof(pthread_t));
	struct io_thread *workers =
		calloc(g_test_ctx.num_threads, sizeof(struct io_thread));

	struct timeval start, end;

	gettimeofday(&start, NULL);
	for (int i = 0; i < g_test_ctx.num_threads; i++) {
		workers[i].worker_id = i;
		workers[i].g_test_ctx = &g_test_ctx;
		workers[i].io_queue_id = io_queue_id++ % g_test_ctx.num_io_queues;
		workers[i].volume_id = volume_id++ % g_test_ctx.num_volumes;
		workers[i].is_write = g_test_ctx.is_write;
		workers[i].queue_size = g_test_ctx.queue_size;
		if (pthread_create(&worker_handle[i], NULL, io_worker, &workers[i])) {
			SPDK_PTL_FATAL("Failed to spawn worker %d", i);
		}
	}
	uint64_t total_ios_completed = 0;
	uint64_t total_ios_submitted = 0;
	for (int i = 0; i < g_test_ctx.num_threads; i++) {
		pthread_join(worker_handle[i], NULL);
		total_ios_completed += workers[i].ios_completed;
		total_ios_submitted += workers[i].ios_submitted;
	}
	gettimeofday(&end, NULL);
	double duration_in_sec = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

	SPDK_PTL_INFO("All workers finished IOs submitted: %lu IOs completed: "
		      "%lu throughput results are: (XXX TODO XXX)",
		      total_ios_submitted, total_ios_completed);
	free(workers);
	free(worker_handle);
	uint64_t total_bytes = total_ios_completed * g_test_ctx.io_size;
	double throughput = (total_bytes / (1024.0 * 1024)) / duration_in_sec;

	SPDK_PTL_INFO(
		"\n=== Test Results ===\n"
		"Throughput: %lf MB/s\n"
		"Total I/O:  %lf MB\n"
		"Duration:   %lf sec\n",
		throughput,
		total_bytes / (1024.0 * 1024),
		duration_in_sec
	);


	// Cleanup
	cleanup_volumes();

	spdk_env_fini();

	return 0;
}
