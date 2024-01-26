#include <catch_config.h>
#include <mpi.h>
#include <test_utils.h>

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <ucp/api/ucp.h>

namespace dyad::test {}
namespace dt = dyad::test;
namespace fs = std::experimental::filesystem;
const uint64_t KB =1024;
const uint64_t MB = 1024 * 1024;
#define UCX_STATUS_FAIL(status) (status != UCS_OK)
/**
 * Test data structures
 */
namespace dyad::test {
struct Arguments {
    size_t request_size = 4096;
    size_t iteration = 64;
    int ranks_per_node = 1;
    int servers_per_node = 1;
    int num_nodes = 1;
    bool debug = false;
};
struct Info {
    int rank;
    int comm_size;
    char hostname[256];
};
}  // namespace dyad::test
dt::Arguments args;
dt::Info info;

/**
 * Overridden methods for catch
 */
int init(int *argc, char ***argv) {
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &info.rank);
    MPI_Comm_size(MPI_COMM_WORLD, &info.comm_size);
    TEST_LOGGER->level = cpplogger::LoggerType::LOG_INFO;
    if (args.debug && info.rank == 0) {
        TEST_LOGGER_INFO("Connect to processes");
        getchar();
    }
    MPI_Barrier(MPI_COMM_WORLD);
    auto test_logger = TEST_LOGGER;
    return 0;
}
int finalize() {
    MPI_Finalize();
    return 0;
}
cl::Parser define_options() {
    return cl::Opt(args.debug, "debug")["--debug"]("Enable debugging.")|
           cl::Opt(args.request_size,
                   "request_size")["--request_size"]("request_size.")  |
           cl::Opt(args.iteration,
                   "iteration")["--iteration"]("iteration.")  |
           cl::Opt(args.ranks_per_node,
                   "ranks_per_node")["--ranks_per_node"]("# ranks per node.") |
           cl::Opt(args.servers_per_node,
                "servers_per_node")["--servers_per_node"]("# servers per node.") |
           cl::Opt(args.num_nodes,
                   "num_nodes")["--num_nodes"]("# num nodes.");
}
/**
 * Helper functions
 */
int pretest() {
    return 0;
}
int posttest() {
    return 0;
}

struct UCXHandle {
    ucp_context_h ucx_ctx;
    ucp_worker_h ucx_worker;
    ucp_mem_h mem_handle;

    ucp_address_t* local_address;
    size_t local_addr_len;
    ucp_address_t* remote_address;
    size_t remote_addr_len;
    ucp_ep_h ep;
    void *buf;
    bool is_sender;
};

struct Exchange {
    ucp_address_t*   recv_addr;
    size_t         recv_addr_len;
    void* buf_ptr;
    void* rkey_buf;
    size_t rkey_size;
};



void ucx_request_init (void* request)
{
    TEST_LOGGER_INFO ("ucx_request_init\n");
}
int allocate_buffer(UCXHandle* ucx_handle, Exchange* exchange) {
    ucs_status_t status;
    TEST_LOGGER_INFO ("Allocating memory with UCX");

    ucp_mem_map_params_t mmap_params;

    mmap_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH
                             | UCP_MEM_MAP_PARAM_FIELD_FLAGS;
    mmap_params.address = NULL;
    mmap_params.length = (1 << 10);
    mmap_params.flags = UCP_MEM_MAP_ALLOCATE;
    status = ucp_mem_map (ucx_handle->ucx_ctx, &mmap_params, &ucx_handle->mem_handle);
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("ucx_mem_map failed");
        return -1;
    }
    TEST_LOGGER_INFO ("Done ucp_mem_map");
    ucp_mem_attr_t attr;
    attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS;
    status = ucp_mem_query (ucx_handle->mem_handle, &attr);
    if (UCX_STATUS_FAIL (status)) {
        ucp_mem_unmap (ucx_handle->ucx_ctx, ucx_handle->mem_handle);
        return -1;
    }
    TEST_LOGGER_INFO ("Done ucp_mem_query");
    ucx_handle->buf = attr.address;
    TEST_LOGGER_INFO ("Done writing address");
    status = ucp_rkey_pack (ucx_handle->ucx_ctx, ucx_handle->mem_handle, &(exchange->rkey_buf), &(exchange->rkey_size));
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("ucp_rkey_pack failed errno %d", status);
        return -1;
    }
    TEST_LOGGER_INFO ("Done ucp_rkey_pack");
    ucp_rkey_h rkey;
    status = ucp_ep_rkey_unpack (ucx_handle->ep, exchange->rkey_buf, &rkey);
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("ucp_ep_rkey_unpack failed");
        return -1;
    }
    char buf[256] = {"hello"};
    ucp_request_param_t params;
    ucs_status_ptr_t request = ucp_put_nbx(ucx_handle->ep, buf, 5, (uint64_t)ucx_handle->buf, rkey, &params);
    TEST_LOGGER_INFO ("Done ucp_put_nbx");
    ucs_status_t final_request_status = UCS_OK;
    // If 'request' is actually a request handle, this means the communication
    // operation is scheduled, but not yet completed.
    if (UCS_PTR_IS_PTR (request)) {
        // Spin lock until the request is completed
        // The spin lock shouldn't be costly (performance-wise)
        // because the wait should always come directly after other UCX calls
        // that minimize the size of the worker's event queue.
        // In other words, prior UCX calls should mean that this loop only runs
        // a couple of times at most.
        do {
            ucp_worker_progress (ucx_handle->ucx_worker);
            // usleep(100);
            // Get the final status of the communication operation
            final_request_status = ucp_request_check_status (request);
        } while (final_request_status == UCS_INPROGRESS);
        // Free and deallocate the request object
        ucp_request_free (request);
    }
        // If 'request' is actually a UCX error, this means the communication
        // operation immediately failed. In that case, we simply grab the
        // 'ucs_status_t' object for the error.
    else if (UCS_PTR_IS_ERR (request)) {
        final_request_status = UCS_PTR_STATUS (request);
    }
    sleep(5);
    printf("%s", (char*)ucx_handle->buf);
    // If 'request' is neither a request handle nor an error, then
    // the communication operation immediately completed successfully.
    // So, we simply set the status to UCS_OK
    final_request_status = UCS_OK;
//    status = ucp_ep_rkey_unpack	(ucx_handle->ep, ucx_handle->rkey_buf, &(ucx_handle->rkey));
//    if (UCX_STATUS_FAIL (status)) {
//        TEST_LOGGER_ERROR ("ucp_ep_rkey_unpack failed");
//        return -1;
//    }
//    status = ucp_rkey_ptr (ucx_handle->rkey, 0, &(ucx_handle->buf));
//    if (UCX_STATUS_FAIL (status)) {
//        TEST_LOGGER_ERROR ( "ucp_ep_rkey_unpack failed");
//        return -1;
//    }
    TEST_LOGGER_INFO ("Done ucp_put_nbx wait");
    return 0;
}
static void ucx_ep_err_handler (void* arg, ucp_ep_h ep, ucs_status_t status)
{
    TEST_LOGGER_ERROR("An error occured on the UCP endpoint (status = %d)", status);
}
int ucx_initialize(UCXHandle** ucx_handle, Exchange* exchange) {
    ucp_params_t ucx_params;
    ucp_worker_params_t worker_params;
    ucp_config_t* config;
    ucs_status_t status;
    ucp_worker_attr_t worker_attrs;

    *ucx_handle = (UCXHandle*)malloc (sizeof (UCXHandle));

    (*ucx_handle)->ucx_ctx = NULL;
    (*ucx_handle)->ucx_worker = NULL;
    (*ucx_handle)->mem_handle = NULL;

    (*ucx_handle)->ep = NULL;
    (*ucx_handle)->local_address = NULL;
    (*ucx_handle)->local_addr_len = 0;
    (*ucx_handle)->remote_address = NULL;
    (*ucx_handle)->remote_addr_len = 0;

    exchange->rkey_buf = NULL;
    exchange->rkey_size = 0;

    // Read the UCX configuration
    TEST_LOGGER_INFO ("Reading UCP config\n");
    status = ucp_config_read (NULL, NULL, &config);
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("Could not read the UCX config\n");
        return -1;
    }
    ucx_params.field_mask =
        UCP_PARAM_FIELD_FEATURES | UCP_PARAM_FIELD_REQUEST_SIZE | UCP_PARAM_FIELD_REQUEST_INIT;
    ucx_params.features = UCP_FEATURE_RMA | UCP_FEATURE_AMO32;
    ucx_params.request_size = (1 << 10);
    ucx_params.request_init = nullptr;

    // Initialize UCX
    TEST_LOGGER_INFO ("Initializing UCP\n");
    status = ucp_init (&ucx_params, config, &((*ucx_handle)->ucx_ctx));

    // Release the config
    ucp_config_release (config);
    // Log an error if UCX initialization failed
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("ucp_init failed (status = %d)\n", status);
        return -1;
    }

    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_MULTI; //Todo(hari): need synchronization between sender and recv else use UCS_THREAD_MODE_SERIALIZED

    // Create the worker and log an error if that fails
    TEST_LOGGER_INFO ("Creating UCP worker\n");
    status = ucp_worker_create ((*ucx_handle)->ucx_ctx, &worker_params, &((*ucx_handle)->ucx_worker));
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("ucp_worker_create failed (status = %d)!\n", status);
        return -1;
    }

    // Query the worker for its address
    worker_attrs.field_mask = UCP_WORKER_ATTR_FIELD_ADDRESS;
    TEST_LOGGER_INFO ("Get address of UCP worker\n");
    status = ucp_worker_query ((*ucx_handle)->ucx_worker, &worker_attrs);
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("Cannot get UCX worker address (status = %d)!\n", status);
        return -1;
    }
    (*ucx_handle)->local_address = worker_attrs.address;
    (*ucx_handle)->local_addr_len = worker_attrs.address_length;

    ucp_ep_params_t params;
    params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    params.address = (*ucx_handle)->local_address;
    TEST_LOGGER_INFO ("Set ucp_ep_create\n");
    status = ucp_ep_create ((*ucx_handle)->ucx_worker, &params, &((*ucx_handle)->ep));
    if (UCX_STATUS_FAIL (status)) {
        TEST_LOGGER_ERROR ("ucp_ep_create failed with status %d", (int)status);
        return -1;
    }
    if ((*ucx_handle)->ep == NULL) {
        TEST_LOGGER_ERROR ("ucp_ep_create succeeded, but returned a NULL endpoint");
        return -1;
    }
    // Allocate a buffer of max transfer size using UCX
    int ret = allocate_buffer(*ucx_handle, exchange);
    (*ucx_handle)->ep = NULL;
    return ret;
}


/**
 * Test cases
 */
TEST_CASE("TestUCXRDMA", CONVERT_VAL(ranks_per_node, args.ranks_per_node) +
                         CONVERT_VAL(servers_per_node, args.servers_per_node) +
                         CONVERT_ENUM(nodes, args.num_nodes)) {
    REQUIRE(pretest() == 0);
    /*REQUIRE(info.comm_size > 2);
    REQUIRE(info.comm_size > args.servers_per_node);
    REQUIRE(args.ranks_per_node > args.servers_per_node);*/
    bool is_server_rank = info.rank >  args.ranks_per_node - args.servers_per_node;
    auto *handle = (UCXHandle*)malloc(sizeof(UCXHandle));
    auto *exchange = (Exchange*)malloc(sizeof(Exchange));
    REQUIRE(ucx_initialize(&handle, exchange) == 0);
    if (is_server_rank) {
        // Server Ranks
        TEST_LOGGER_INFO("Rank %d is Server", info.rank);

    } else {
        // Client Ranks
        TEST_LOGGER_INFO("Rank %d is Client", info.rank);

    }
    REQUIRE(posttest() == 0);
}