#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/dtl/ucx_dtl.h>

#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/utils/base64/base64.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern const base64_maps_t base64_maps_rfc4648;

#define UCX_MAX_TRANSFER_SIZE (1024 * 1024 * 1024)

// Tag mask for UCX Tag send/recv
#define DYAD_UCX_TAG_MASK UINT64_MAX

// Define a request struct to be used in handling
// async UCX operations
struct ucx_request {
    int completed;
};
typedef struct ucx_request dyad_ucx_request_t;

// Define a function that UCX will use to allocate and
// initialize our request struct
static void dyad_ucx_request_init (void* request)
{
    DYAD_C_FUNCTION_START();
    dyad_ucx_request_t* real_request = NULL;
    real_request = (dyad_ucx_request_t*)request;
    real_request->completed = 0;
    DYAD_C_FUNCTION_END();
}

// Define a function that ucp_tag_msg_recv_nbx will use
// as a callback to signal the completion of the async receive
// TODO(Ian): See if we can get msg size from recv_info
#if UCP_API_VERSION >= UCP_VERSION(1, 10)
static void dyad_recv_callback (void* request,
                                ucs_status_t status,
                                const ucp_tag_recv_info_t* tag_info,
                                void* user_data)
#else
static void dyad_recv_callback (void* request, ucs_status_t status, ucp_tag_recv_info_t* tag_info)
#endif
{
    DYAD_C_FUNCTION_START();
    dyad_ucx_request_t* real_request = NULL;
    real_request = (dyad_ucx_request_t*)request;
    real_request->completed = 1;
    DYAD_C_FUNCTION_END();
}

#if UCP_API_VERSION >= UCP_VERSION(1, 10)
static void dyad_send_callback (void* req, ucs_status_t status, void* ctx)
#else
static void dyad_send_callback (void* req, ucs_status_t status)
#endif
{
    DYAD_C_FUNCTION_START();
    dyad_ucx_request_t* real_req = (dyad_ucx_request_t*)req;
    real_req->completed = 1;
    DYAD_C_FUNCTION_END();
}

static void dyad_ucx_ep_err_handler (void* arg, ucp_ep_h ep, ucs_status_t status)
{
    DYAD_C_FUNCTION_START();
    dyad_ctx_t *ctx = (dyad_ctx_t*)arg;
    DYAD_LOG_ERROR (ctx, "An error occured on the UCP endpoint (status = %d)", status);
    (void) ctx;
    DYAD_C_FUNCTION_END();
}

// Simple function used to wait on the async receive
static ucs_status_t dyad_ucx_request_wait (const dyad_ctx_t* ctx,
                                           dyad_ucx_request_t* request)
{
    DYAD_C_FUNCTION_START();
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
            ucp_worker_progress (ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker);
            // usleep(100);
            // Get the final status of the communication operation
            final_request_status = ucp_request_check_status (request);
        } while (final_request_status == UCS_INPROGRESS);
        // Free and deallocate the request object
        ucp_request_free (request);
        goto dtl_ucx_request_wait_region_finish;
    }
    // If 'request' is actually a UCX error, this means the communication
    // operation immediately failed. In that case, we simply grab the
    // 'ucs_status_t' object for the error.
    else if (UCS_PTR_IS_ERR (request)) {
        final_request_status = UCS_PTR_STATUS (request);
        goto dtl_ucx_request_wait_region_finish;
    }
    // If 'request' is neither a request handle nor an error, then
    // the communication operation immediately completed successfully.
    // So, we simply set the status to UCS_OK
    final_request_status = UCS_OK;
dtl_ucx_request_wait_region_finish:;
    DYAD_C_FUNCTION_END();
    return final_request_status;
}

static dyad_rc_t ucx_allocate_buffer (const dyad_ctx_t *ctx,
                                      ucp_context_h ucp_ctx,
                                      size_t buf_size,
                                      dyad_dtl_comm_mode_t comm_mode,
                                      ucp_mem_h mem_handle,
                                      void** buf)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_BADBUF;
    ucs_status_t status;
    ucp_mem_map_params_t mmap_params;
    ucp_mem_attr_t attr;
    if (ucp_ctx == NULL) {
        rc = DYAD_RC_NOCTX;
        DYAD_LOG_ERROR (ctx, "No UCX context provided");
        goto ucx_allocate_done;
    }
    if (buf == NULL) {
        rc = DYAD_RC_BADBUF;
        DYAD_LOG_ERROR (ctx, "Bad data buffer pointer provided");
        goto ucx_allocate_done;
    }
    if (comm_mode == DYAD_COMM_NONE) {
        rc = DYAD_RC_BAD_COMM_MODE;
        goto ucx_allocate_done;
    }
    DYAD_LOG_INFO (ctx, "Allocating memory with UCX");
    mmap_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH
                             | UCP_MEM_MAP_PARAM_FIELD_FLAGS | UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    mmap_params.address = NULL;
    mmap_params.memory_type = UCS_MEMORY_TYPE_HOST;
    mmap_params.length = buf_size + sizeof(ssize_t);
    mmap_params.flags = UCP_MEM_MAP_ALLOCATE;
    status = ucp_mem_map (ucp_ctx, &mmap_params, &mem_handle);
    if (UCX_STATUS_FAIL (status)) {
        rc = DYAD_RC_UCXMMAP_FAIL;
        DYAD_LOG_ERROR (ctx, "ucx_mem_map failed");
        goto ucx_allocate_done;
    }
    attr.field_mask = UCP_MEM_ATTR_FIELD_ADDRESS;
    status = ucp_mem_query (mem_handle, &attr);
    if (UCX_STATUS_FAIL (status)) {
        ucp_mem_unmap (ucp_ctx, mem_handle);
        rc = DYAD_RC_UCXMMAP_FAIL;
        DYAD_LOG_ERROR (ctx, "Failed to get address to UCX allocated buffer");
        goto ucx_allocate_done;
    }
    *buf = attr.address;
    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    ucp_memh_pack_params_t params;
    params.field_mask = UCP_MEMH_PACK_PARAM_FIELD_FLAGS;
    params.flags = UCP_MEMH_PACK_FLAG_EXPORT;
    status = ucp_memh_pack(mem_handle, &params, &(dtl_handle->rkey_buf), &(dtl_handle->rkey_size));
    if (UCX_STATUS_FAIL (status)) {
        rc = DYAD_RC_UCXRKEY_FAIL;
        DYAD_LOG_ERROR (ctx, "ucp_rkey_pack failed");
        goto ucx_allocate_done;
    }
    rc = DYAD_RC_OK;

ucx_allocate_done:;
    DYAD_C_FUNCTION_END();
    return rc;
}

static dyad_rc_t ucx_free_buffer (const dyad_ctx_t *ctx,
                                  ucp_context_h ucp_ctx,
                                  ucp_mem_h mem_handle,
                                  void** buf)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    if (ucp_ctx == NULL) {
        rc = DYAD_RC_NOCTX;
        DYAD_LOG_ERROR (ctx, "No UCX context provided");
        goto ucx_free_buf_done;
    }
    if (mem_handle == NULL) {
        rc = DYAD_RC_UCXMMAP_FAIL;
        DYAD_LOG_ERROR (ctx, "No UCX memory handle provided");
        goto ucx_free_buf_done;
    }
    if (buf == NULL || *buf == NULL) {
        rc = DYAD_RC_BADBUF;
        DYAD_LOG_ERROR (ctx, "No memory buffer provided");
        goto ucx_free_buf_done;
    }
    DYAD_LOG_INFO (ctx, "Releasing UCX allocated memory");
    ucp_mem_unmap (ucp_ctx, mem_handle);
    *buf = NULL;
    rc = DYAD_RC_OK;

ucx_free_buf_done:;
    DYAD_C_FUNCTION_END();
    return rc;
}

static inline ucs_status_ptr_t ucx_send_no_wait (const dyad_ctx_t* ctx, bool is_warmup, void* buf, size_t buflen)
{
    DYAD_C_FUNCTION_START();
    ucs_status_ptr_t stat_ptr;
    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    if (dtl_handle->ep == NULL) {
        DYAD_LOG_ERROR (ctx,
                       "UCP endpoint was not created prior to invoking "
                       "send!");
        stat_ptr = (void*)UCS_ERR_NOT_CONNECTED;
        goto ucx_send_no_wait_done;
    }
    ucp_rkey_h rkey;
    ucs_status_t status = ucp_ep_rkey_unpack (dtl_handle->ep, dtl_handle->rkey_buf, &rkey);
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "ucp_ep_rkey_unpack failed");
        return stat_ptr;
    }
    ucp_request_param_t params;
    params.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK;
    params.cb.send = dyad_send_callback;
    DYAD_LOG_INFO (ctx, "Sending data to consumer with ucp_put_nbx");
    memcpy (buf+dtl_handle->max_transfer_size, &buflen, sizeof(buflen));
    ucp_address_t* address;
    if (is_warmup) address = dtl_handle->local_address;
    else address = dtl_handle->remote_address;
    stat_ptr = ucp_put_nbx(dtl_handle->ep, buf, buflen, (uint64_t)address, rkey, &params);
ucx_send_no_wait_done:;
    DYAD_C_FUNCTION_END();
    return stat_ptr;
}

static inline ucs_status_ptr_t ucx_recv_no_wait (const dyad_ctx_t* ctx,
                                                 bool is_warmup,
                                                 void** buf,
                                                 size_t* buflen)
{
    DYAD_C_FUNCTION_START();
    ucs_status_ptr_t stat_ptr = NULL;
//    dyad_rc_t rc = DYAD_RC_OK;
//    ucp_tag_message_h msg = NULL;
//    ucp_tag_recv_info_t msg_info;
//
//    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
//    DYAD_LOG_INFO (ctx, "Poll UCP for incoming data");
//    // TODO: replace this loop with a resiliency response over RPC
//    // TODO(Ian): explore whether removing probe makes the overall
//    //            recv faster or not
//    do {
//        ucp_worker_progress (dtl_handle->ucx_worker);
//        msg = ucp_tag_probe_nb (dtl_handle->ucx_worker,
//                                dtl_handle->comm_tag,
//                                DYAD_UCX_TAG_MASK,
//                                1,  // Remove the message from UCP tracking
//                                // Requires calling ucp_tag_msg_recv_nb
//                                // with the ucp_tag_message_h to retrieve message
//                                &msg_info);
//    } while (msg == NULL);
//    // TODO: This version of the polling code is not supposed to spin-lock,
//    // unlike the code above. Currently, it does not work. Once it starts
//    // working, we can replace the code above with a version of this code.
//    //
//    // while (true)
//    // {
//    //     // Probe the tag recv event at the top
//    //     // of the worker's queue
//    //     DYAD_LOG_INFO (ctx, "Probe UCP worker with tag %lu",
//    //     dtl_handle->comm_tag); msg = ucp_tag_probe_nb(
//    //         dtl_handle->ucx_worker,
//    //         dtl_handle->comm_tag,
//    //         DYAD_UCX_TAG_MASK,
//    //         1, // Remove the message from UCP tracking
//    //         // Requires calling ucp_tag_msg_recv_nb
//    //         // with the ucp_tag_message_h to retrieve message
//    //         &msg_info
//    //     );
//    //     // If data has arrived from the producer plugin,
//    //     // break the loop
//    //     if (msg != NULL)
//    //     {
//    //         DYAD_LOG_INFO (ctx, "Data has arrived, so end
//    //         polling"); break;
//    //     }
//    //     // If data has not arrived, check if there are
//    //     // any other events in the worker's queue.
//    //     // If so, start the loop over to handle the next event
//    //     else if (ucp_worker_progress(dtl_handle->ucx_worker))
//    //     {
//    //         DYAD_LOG_INFO (ctx, "Progressed UCP worker to check if
//    //         any other UCP events are available"); continue;
//    //     }
//    //     // No other events are queued. So, we will wait on new
//    //     // events to come in. By using 'ucp_worker_wait' for this,
//    //     // we let the OS do other work in the meantime (no spin locking).
//    //     DYAD_LOG_INFO (ctx, "Launch pre-emptable wait until UCP
//    //     worker gets new events\n"); status =
//    //     ucp_worker_wait(dtl_handle->ucx_worker);
//    //     // If the wait fails, log an error
//    //     if (UCX_STATUS_FAIL(status))
//    //     {
//    //         DYAD_LOG_INFO (ctx, "Could not wait on the message from
//    //         the producer plugin\n"); return DYAD_RC_UCXWAIT_FAIL;
//    //     }
//    // }
//    // The metadata retrived from the probed tag recv event contains
//    // the size of the data to be sent.
//    // So, use that size to allocate a buffer
//    DYAD_LOG_INFO (ctx,
//                   "Got message with tag %lu and size %lu\n",
//                   msg_info.sender_tag,
//                   msg_info.length);
//    *buflen = msg_info.length;
//    if (is_warmup) {
//        if (*buflen > dtl_handle->max_transfer_size) {
//            *buflen = 0;
//            stat_ptr = (ucs_status_ptr_t)UCS_ERR_BUFFER_TOO_SMALL;
//            goto ucx_recv_no_wait_done;
//        }
//    } else {
//        rc = ctx->dtl_handle->get_buffer (ctx, *buflen, buf);
//        if (DYAD_IS_ERROR (rc)) {
//            *buf = NULL;
//            *buflen = 0;
//            stat_ptr = (ucs_status_ptr_t)UCS_ERR_NO_MEMORY;
//            goto ucx_recv_no_wait_done;
//        }
//    }
//    DYAD_LOG_INFO (ctx, "Receive data using async UCX operation\n");
//#if UCP_API_VERSION >= UCP_VERSION(1, 10)
//    // Define the settings for the recv operation
//    //
//    // The settings enabled are:
//    //   * Define callback for the recv because it is async
//    //   * Restrict memory buffers to host-only since we aren't directly
//    //     dealing with GPU memory
//    ucp_request_param_t recv_params;
//    // TODO consider enabling UCP_OP_ATTR_FIELD_MEMH to speedup
//    // the recv operation if using RMA behind the scenes
//    recv_params.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE;
//    recv_params.cb.recv = dyad_recv_callback;
//    // Constraining to Host memory (as opposed to GPU memory)
//    // allows UCX to potentially perform some optimizations
//    recv_params.memory_type = UCS_MEMORY_TYPE_HOST;
//    // Perform the async recv operation using the probed tag recv event
//    stat_ptr = ucp_tag_msg_recv_nbx (dtl_handle->ucx_worker, *buf, *buflen, msg, &recv_params);
//#else
//    stat_ptr = ucp_tag_msg_recv_nb (dtl_handle->ucx_worker,
//                                    *buf,
//                                    *buflen,
//                                    UCP_DATATYPE_CONTIG,
//                                    msg,
//                                    dyad_recv_callback);
//#endif
//ucx_recv_no_wait_done:;
    DYAD_C_FUNCTION_END();
    return stat_ptr;
}

static dyad_rc_t ucx_warmup (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    void* send_buf = NULL;
    void* recv_buf = NULL;
    size_t recv_buf_len = 0;
    ucs_status_ptr_t send_stat_ptr = NULL;
    ucs_status_ptr_t recv_stat_ptr = NULL;
    ucs_status_t send_status = UCS_OK;
    ucs_status_t recv_status = UCS_OK;
    DYAD_LOG_INFO (ctx, "Starting warmup for UCX DTL");
    DYAD_LOG_INFO (ctx, "Getting buffers for sending and receiving");
    rc = dyad_dtl_ucx_get_buffer (ctx, 1, &send_buf);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Failed to get UCX-allocated buffer");
        goto warmup_region_done;
    }
    recv_buf = malloc (1);
    if (recv_buf == NULL) {
        dyad_dtl_ucx_return_buffer (ctx, &send_buf);
        rc = DYAD_RC_SYSFAIL;
        DYAD_LOG_ERROR (ctx, "Failed to allocate receive buffer for warmup");
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Establishing connection with self");
    rc = ucx_connect (ctx,
                      ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker,
                      ctx->dtl_handle->private_dtl.ucx_dtl_handle->local_address,
                      &(ctx->dtl_handle->private_dtl.ucx_dtl_handle->ep));
    if (DYAD_IS_ERROR (rc)) {
        free (recv_buf);
        dyad_dtl_ucx_return_buffer (ctx, &send_buf);
        DYAD_LOG_ERROR (ctx, "Failed to establish connection with self");
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Starting non-blocking send for warmup");
    send_stat_ptr = ucx_send_no_wait (ctx, true, send_buf, 1);
    if ((uintptr_t)send_stat_ptr == (uintptr_t)UCS_ERR_NOT_CONNECTED) {
        DYAD_LOG_ERROR (ctx, "Send failed because there's no endpoint");
        free (recv_buf);
        dyad_dtl_ucx_return_buffer (ctx, &send_buf);
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Starting non-blocking recv for warmup");
    recv_stat_ptr = ucx_recv_no_wait (ctx, true, &recv_buf, &recv_buf_len);
    DYAD_LOG_INFO (ctx, "Waiting on warmup send to finish");
    send_status =
        dyad_ucx_request_wait (ctx, send_stat_ptr);
    DYAD_LOG_INFO (ctx, "Disconnecting from self");
    ucx_disconnect (ctx,
                    ctx->dtl_handle->private_dtl.ucx_dtl_handle->ucx_worker,
                    ctx->dtl_handle->private_dtl.ucx_dtl_handle->ep);
    dyad_dtl_ucx_return_buffer (ctx, &send_buf);
    if (UCX_STATUS_FAIL (recv_status) || UCX_STATUS_FAIL (send_status)) {
        rc = DYAD_RC_UCXCOMM_FAIL;
        DYAD_LOG_ERROR (ctx, "Warmup communication failed in UCX!");
        goto warmup_region_done;
    }
    DYAD_LOG_INFO (ctx, "Communication succeeded (according to UCX)");
    assert (recv_buf_len == 1);
    DYAD_LOG_INFO (ctx, "Correct amount of data received in warmup");
    free (recv_buf);
    rc = DYAD_RC_OK;

warmup_region_done:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_init (const dyad_ctx_t* ctx,
                             dyad_dtl_mode_t mode,
                             dyad_dtl_comm_mode_t comm_mode,
                             bool debug)
{
    DYAD_C_FUNCTION_START();
    ucp_params_t ucx_params;
    ucp_worker_params_t worker_params;
    ucp_config_t* config;
    ucs_status_t status;
    ucp_worker_attr_t worker_attrs;
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t* dtl_handle = NULL;

    ctx->dtl_handle->private_dtl.ucx_dtl_handle = malloc (sizeof (struct dyad_dtl_ucx));
    if (ctx->dtl_handle->private_dtl.ucx_dtl_handle == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate UCX DTL context\n");
        DYAD_C_FUNCTION_END();
        return DYAD_RC_SYSFAIL;
    }
    dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    // Allocation/Freeing of the Flux handle should be
    // handled by the DYAD context
    dtl_handle->h = ctx->h;
    dtl_handle->comm_mode = comm_mode;
    dtl_handle->debug = debug;
    dtl_handle->ucx_ctx = NULL;
    dtl_handle->ucx_worker = NULL;
    dtl_handle->mem_handle = NULL;
    dtl_handle->net_buf = NULL;
    dtl_handle->max_transfer_size = UCX_MAX_TRANSFER_SIZE;
    dtl_handle->ep = NULL;
    dtl_handle->ep_cache = NULL;
    dtl_handle->local_address = NULL;
    dtl_handle->local_addr_len = 0;
    dtl_handle->remote_address = NULL;
    dtl_handle->remote_addr_len = 0;
    dtl_handle->comm_tag = 0;

    // Read the UCX configuration
    DYAD_LOG_INFO (ctx, "Reading UCP config\n");
    status = ucp_config_read (NULL, NULL, &config);
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "Could not read the UCX config\n");
        goto error;
    }

    // Define the settings, parameters, features, etc.
    // for the UCX context. UCX will use this info internally
    // when creating workers, endpoints, etc.
    //
    // The settings enabled are:
    //   * Tag-matching send/recv
    //   * Remote Memory Access communication
    //   * Auto initialization of request objects
    //   * Worker sleep, wakeup, poll, etc. features
    ucx_params.field_mask =
        UCP_PARAM_FIELD_FEATURES | UCP_PARAM_FIELD_REQUEST_SIZE | UCP_PARAM_FIELD_REQUEST_INIT;
    ucx_params.features = UCP_FEATURE_TAG |
                          // UCP_FEATURE_RMA |
                          UCP_FEATURE_WAKEUP;
    ucx_params.request_size = sizeof (struct ucx_request);
    ucx_params.request_init = dyad_ucx_request_init;

    // Initialize UCX
    DYAD_LOG_INFO (ctx, "Initializing UCP\n");
    status = ucp_init (&ucx_params, config, &dtl_handle->ucx_ctx);

    // If in debug mode, print the configuration of UCX to stderr
    if (debug) {
        ucp_config_print (config, stderr, "UCX Configuration", UCS_CONFIG_PRINT_CONFIG);
    }
    // Release the config
    ucp_config_release (config);
    // Log an error if UCX initialization failed
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "ucp_init failed (status = %d)\n", status);
        goto error;
    }

    // Define the settings for the UCX worker (i.e., progress engine)
    //
    // The settings enabled are:
    //   * Single-threaded mode (TODO look into multi-threading support)
    //   * Restricting wakeup events to only include Tag-matching recv events
    worker_params.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE | UCP_WORKER_PARAM_FIELD_EVENTS;
    // TODO look into multi-threading support
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
    worker_params.events = UCP_WAKEUP_TAG_RECV;

    // Create the worker and log an error if that fails
    DYAD_LOG_INFO (ctx, "Creating UCP worker\n");
    status = ucp_worker_create (dtl_handle->ucx_ctx, &worker_params, &(dtl_handle->ucx_worker));
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "ucp_worker_create failed (status = %d)!\n", status);
        goto error;
    }

    // Query the worker for its address
    worker_attrs.field_mask = UCP_WORKER_ATTR_FIELD_ADDRESS;
    DYAD_LOG_INFO (ctx, "Get address of UCP worker\n");
    status = ucp_worker_query (dtl_handle->ucx_worker, &worker_attrs);
    if (UCX_STATUS_FAIL (status)) {
        DYAD_LOG_ERROR (ctx, "Cannot get UCX worker address (status = %d)!\n", status);
        goto error;
    }
    dtl_handle->local_address = worker_attrs.address;
    dtl_handle->local_addr_len = worker_attrs.address_length;

    // Initialize endpoint cache
    rc = dyad_ucx_ep_cache_init (ctx, &(dtl_handle->ep_cache));
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Cannot create endpoint cache (err code = %d)", (int)rc);
        goto error;
    }

    // Allocate a buffer of max transfer size using UCX
    ucx_allocate_buffer (ctx,
                         dtl_handle->ucx_ctx,
                         dtl_handle->max_transfer_size,
                         dtl_handle->comm_mode,
                         dtl_handle->mem_handle,
                         &(dtl_handle->net_buf));

    ctx->dtl_handle->rpc_pack = dyad_dtl_ucx_rpc_pack;
    ctx->dtl_handle->rpc_unpack = dyad_dtl_ucx_rpc_unpack;
    ctx->dtl_handle->rpc_respond = dyad_dtl_ucx_rpc_respond;
    ctx->dtl_handle->rpc_recv_response = dyad_dtl_ucx_rpc_recv_response;
    ctx->dtl_handle->get_buffer = dyad_dtl_ucx_get_buffer;
    ctx->dtl_handle->return_buffer = dyad_dtl_ucx_return_buffer;
    ctx->dtl_handle->establish_connection = dyad_dtl_ucx_establish_connection;
    ctx->dtl_handle->send = dyad_dtl_ucx_send;
    ctx->dtl_handle->recv = dyad_dtl_ucx_recv;
    ctx->dtl_handle->close_connection = dyad_dtl_ucx_close_connection;

    rc = ucx_warmup (ctx);
    if (DYAD_IS_ERROR (rc)) {
        DYAD_LOG_ERROR (ctx, "Warmup for UCX DTL failed");
        goto error;
    }
    dtl_handle->ep = NULL;

    DYAD_C_FUNCTION_END();

    return DYAD_RC_OK;

error:;
    // If an error occured, finalize the DTL handle and
    // return a failing error code
    dyad_dtl_ucx_finalize (ctx);
    DYAD_C_FUNCTION_END();
    return DYAD_RC_UCXINIT_FAIL;
}

dyad_rc_t dyad_dtl_ucx_rpc_pack (const dyad_ctx_t* ctx,
                                 const char* restrict upath,
                                 uint32_t producer_rank,
                                 json_t** restrict packed_obj)
{
    DYAD_C_FUNCTION_START();
    DYAD_C_FUNCTION_UPDATE_STR ("upath", upath);
    DYAD_C_FUNCTION_UPDATE_INT ("producer_rank", producer_rank);
    DYAD_C_FUNCTION_UPDATE_INT ("pid", ctx->pid);
    dyad_rc_t rc = DYAD_RC_OK;
    size_t encoded_address_len = 0;
    char* encoded_address_buf = NULL;
    ssize_t enc_size = 0;
    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    if (dtl_handle->local_address == NULL) {
        DYAD_LOG_ERROR (dtl_handle, "Tried to pack an RPC payload without a local UCX address");
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    DYAD_LOG_DEBUG (ctx, "Encode UCP address using base64\n");
    encoded_address_len = base64_encoded_length (dtl_handle->local_addr_len);
    // Add 1 to encoded length because the encoded buffer will be
    // packed as if it is a string
    encoded_address_buf = malloc (encoded_address_len + 1);
    DYAD_LOG_DEBUG(ctx, "Encode UCP address len %d", encoded_address_len);
    if (encoded_address_buf == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate buffer for packed address\n");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    // remote_address is casted to const char* to avoid warnings
    // This is valid because it is a pointer to an opaque struct,
    // so the cast can be treated like a void*->char* cast.
    enc_size = base64_encode_using_maps (&base64_maps_rfc4648,
                                         encoded_address_buf,
                                         encoded_address_len + 1,
                                         (const char*)dtl_handle->local_address,
                                         dtl_handle->local_addr_len);
    DYAD_LOG_DEBUG(ctx, "Encode UCP address size %d", enc_size);
    if (enc_size < 0) {
        // TODO log error
        DYAD_LOG_ERROR (ctx, "Could not encode local address\n");
        free (encoded_address_buf);
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }

    DYAD_LOG_DEBUG (ctx, "Encode UCP rkey using base64\n");
    size_t encoded_rkey_len = 0;
    char* encoded_rkey_buf = NULL;
    encoded_rkey_len = base64_encoded_length (dtl_handle->rkey_size);
    // Add 1 to encoded length because the encoded buffer will be
    // packed as if it is a string
    encoded_rkey_buf = malloc (encoded_rkey_len + 1);
    DYAD_LOG_DEBUG(ctx, "Encode UCP rkey len %d", encoded_rkey_len);
    if (encoded_rkey_buf == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate buffer for rkey\n");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    // remote_address is casted to const char* to avoid warnings
    // This is valid because it is a pointer to an opaque struct,
    // so the cast can be treated like a void*->char* cast.
    enc_size = base64_encode_using_maps (&base64_maps_rfc4648,
                                         encoded_rkey_buf,
                                         encoded_rkey_len + 1,
                                         (const char*)dtl_handle->rkey_buf,
                                         dtl_handle->rkey_size);
    DYAD_LOG_DEBUG(ctx, "Encode UCP rkey size %d", enc_size);
    if (enc_size < 0) {
        DYAD_LOG_ERROR (ctx, "Could not encode rkey address\n");
        // TODO log error
        free (encoded_rkey_buf);
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }

    DYAD_LOG_INFO (ctx, "Creating UCP tag for tag matching\n");
    // Because we're using tag-matching send/recv for communication,
    // there's no need to do any real connection establishment here.
    // Instead, we use this function to create the tag that will be
    // used for the upcoming communication.
    uint32_t consumer_rank = 0;
    if (flux_get_rank (dtl_handle->h, &consumer_rank) < 0) {
        DYAD_LOG_ERROR (ctx, "Cannot get consumer rank\n");
        rc = DYAD_RC_FLUXFAIL;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    DYAD_C_FUNCTION_UPDATE_INT ("consumer_rank", consumer_rank);
    // The tag is a 64 bit unsigned integer consisting of the
    // 32-bit rank of the producer followed by the 32-bit rank
    // of the consumer
    dtl_handle->comm_tag = ((uint64_t)producer_rank << 32) | (uint64_t)consumer_rank;
    // Use Jansson to pack the tag and UCX address into
    // the payload to be sent via RPC to the producer plugin
    DYAD_LOG_INFO (ctx, "Packing RPC payload for UCX DTL\n");
    *packed_obj = json_pack ("{s:s, s:i, s:i, s:i, s:s, s:i, s:s%}",
                             "upath",
                             upath,
                             "tag_prod",
                             (int)producer_rank,
                             "tag_cons",
                             (int)consumer_rank,
                             "pid_cons",
                             ctx->pid,
                             "ucx_addr",
                             encoded_address_buf,
                             "ucx_addr_len",
                             encoded_address_len,
                             "ucx_rkey",
                             encoded_rkey_buf,
                             encoded_rkey_len);
    free (encoded_address_buf);
    free (encoded_rkey_buf);
    // If the packing failed, log an error
    if (*packed_obj == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not pack upath and UCX address for RPC\n");
        rc = DYAD_RC_BADPACK;
        goto dtl_ucx_rpc_pack_region_finish;
    }
    rc = DYAD_RC_OK;
dtl_ucx_rpc_pack_region_finish:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_rpc_unpack (const dyad_ctx_t* ctx, const flux_msg_t* msg, char** upath)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    char* encoded_address_buf = NULL;
    size_t encoded_address_len = 0;
    char* encoded_rkey_buf = NULL;
    size_t encoded_rkey_len = 0;
    int errcode = 0;
    uint64_t tag_prod = 0;
    uint64_t tag_cons = 0;
    uint64_t pid = 0;
    ssize_t decoded_address_len = 0;
    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    DYAD_LOG_INFO (ctx, "Unpacking RPC payload\n");
    errcode = flux_request_unpack (msg,
                                   NULL,
                                   "{s:s, s:i, s:i, s:i, s:s, s:i, s:s%}",
                                   "upath",
                                   upath,
                                   "tag_prod",
                                   &tag_prod,
                                   "tag_cons",
                                   &tag_cons,
                                   "pid_cons",
                                    &pid,
                                   "ucx_addr",
                                   &encoded_address_buf,
                                   "ucx_addr_len",
                                   &encoded_address_len,
                                   "ucx_rkey",
                                   &encoded_rkey_buf,
                                   &encoded_rkey_len);
    if (errcode < 0) {
        DYAD_LOG_ERROR (ctx, "Could not unpack Flux message from consumer!\n");
        rc = DYAD_RC_BADUNPACK;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    DYAD_C_FUNCTION_UPDATE_INT ("pid", pid);
    DYAD_C_FUNCTION_UPDATE_INT ("tag_cons", tag_cons);
    dtl_handle->comm_tag = tag_prod << 32 | tag_cons;
    dtl_handle->consumer_conn_key = pid << 32 | tag_cons;
    DYAD_C_FUNCTION_UPDATE_INT ("cons_key", dtl_handle->consumer_conn_key);
    DYAD_LOG_INFO (ctx, "Obtained upath from RPC payload: %s\n", *upath);
    DYAD_LOG_INFO (ctx, "Obtained UCP tag from RPC payload: %lu\n", dtl_handle->comm_tag);
    DYAD_LOG_INFO (ctx, "Decoding consumer UCP address using base64\n");
    dtl_handle->remote_addr_len = base64_decoded_length (encoded_address_len);
    dtl_handle->remote_address = (ucp_address_t*)malloc (dtl_handle->remote_addr_len);
    if (dtl_handle->remote_address == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate memory for consumer address");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    decoded_address_len = base64_decode_using_maps (&base64_maps_rfc4648,
                                            (char*)dtl_handle->remote_address,
                                            dtl_handle->remote_addr_len,
                                            encoded_address_buf,
                                            encoded_address_len);
    if (decoded_address_len < 0) {
        DYAD_LOG_ERROR (dtl_handle, "Failed to decode remote address");
        free (dtl_handle->remote_address);
        dtl_handle->remote_address = NULL;
        dtl_handle->remote_addr_len = 0;
        rc = DYAD_RC_BAD_B64DECODE;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    dtl_handle->rkey_size = base64_decoded_length (encoded_rkey_len);
    dtl_handle->rkey_buf = (ucp_address_t*)malloc (dtl_handle->rkey_size);
    ssize_t decoded_rkey_len = 0;
    if (dtl_handle->rkey_buf == NULL) {
        DYAD_LOG_ERROR (ctx, "Could not allocate memory for consumer rkey");
        rc = DYAD_RC_SYSFAIL;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    decoded_rkey_len = base64_decode_using_maps (&base64_maps_rfc4648,
                                            (char*)dtl_handle->rkey_buf,
                                            dtl_handle->rkey_size,
                                            encoded_rkey_buf,
                                            encoded_rkey_len);
    if (decoded_rkey_len < 0) {
        DYAD_LOG_ERROR (dtl_handle, "Failed to decode remote rkey");
        free (dtl_handle->rkey_buf);
        dtl_handle->rkey_buf = NULL;
        dtl_handle->rkey_size = 0;
        rc = DYAD_RC_BAD_B64DECODE;
        goto dtl_ucx_rpc_unpack_region_finish;
    }
    rc = DYAD_RC_OK;
dtl_ucx_rpc_unpack_region_finish:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_rpc_respond (const dyad_ctx_t* ctx, const flux_msg_t* orig_msg)
{
    DYAD_C_FUNCTION_START();
    DYAD_C_FUNCTION_END();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_rpc_recv_response (const dyad_ctx_t* ctx, flux_future_t* f)
{
    DYAD_C_FUNCTION_START();
    DYAD_C_FUNCTION_END();
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_get_buffer (const dyad_ctx_t* ctx, size_t data_size, void** data_buf)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    DYAD_LOG_INFO (dtl_handle, "Validating data_buf in get_buffer");
    // TODO(Ian): the second part of this check is (for some reason) evaluating
    //            to true despite `data_buf` being a pointer to a NULL pointer.
    //            Since this function is internal, just comment this piece out for now.
    //            In the future, figure out why this is failing and fix it.
    // if (data_buf == NULL || *data_buf != NULL) {
    //     DYAD_LOG_ERROR (dtl_handle, "Invalid buffer pointer provided");
    //     DYAD_LOG_ERROR (dtl_handle, "Is double pointer NULL? -> %d", (data_buf == NULL));
    //     DYAD_LOG_ERROR (dtl_handle, "Is single pointer not NULL? -> %d", (*data_buf != NULL));
    //     rc = DYAD_RC_BADBUF;
    //     goto ucx_get_buffer_done;
    // }
    DYAD_LOG_INFO (dtl_handle, "Validating data_size in get_buffer");
    if (data_size > ctx->dtl_handle->private_dtl.ucx_dtl_handle->max_transfer_size) {
        DYAD_LOG_ERROR (dtl_handle,
                      "Requested a data size that's larger than the pre-allocated UCX buffer");
        rc = DYAD_RC_BADBUF;
        goto ucx_get_buffer_done;
    }
    DYAD_LOG_INFO (dtl_handle, "Setting the data buffer pointer to the UCX-allocated buffer");
    *data_buf = dtl_handle->net_buf;
    rc = DYAD_RC_OK;

ucx_get_buffer_done:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_return_buffer (const dyad_ctx_t* ctx, void** data_buf)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    if (data_buf == NULL || *data_buf == NULL) {
        rc = DYAD_RC_BADBUF;
        goto dtl_ucx_return_buffer_done;
    }
    *data_buf = NULL;
dtl_ucx_return_buffer_done:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_establish_connection (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    dyad_dtl_comm_mode_t comm_mode = dtl_handle->comm_mode;
    if (comm_mode == DYAD_COMM_SEND) {
        DYAD_LOG_INFO (ctx, "Create UCP endpoint for communication with consumer\n");
        rc = dyad_ucx_ep_cache_find (ctx, dtl_handle->ep_cache,
                                     dtl_handle->remote_address,
                                     dtl_handle->remote_addr_len,
                                     &(dtl_handle->ep));
        if (DYAD_IS_ERROR (rc)) {
            rc = dyad_ucx_ep_cache_insert (ctx,
                                           dtl_handle->ep_cache,
                                           dtl_handle->remote_address,
                                           dtl_handle->remote_addr_len,
                                           dtl_handle->ucx_worker);
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx, "Failed to create UCP endpoint");
                goto dtl_ucx_establish_connection_region_finish;
            }
            rc = dyad_ucx_ep_cache_find (ctx, dtl_handle->ep_cache,
                                         dtl_handle->remote_address,
                                         dtl_handle->remote_addr_len,
                                         &(dtl_handle->ep));
            if (DYAD_IS_ERROR (rc)) {
                DYAD_LOG_ERROR (ctx,
                              "Successfully created UCP endpoint, but can't retrieve it from cache "
                              "(return code = %d)",
                              (int)rc);
                goto dtl_ucx_establish_connection_region_finish;
            }
        }
        // rc = ucx_connect (ctx,
        //                   dtl_handle->ucx_worker,
        //                   dtl_handle->remote_address,
        //                   dtl_handle->h,
        //                   &(dtl_handle->ep));
        // if (DYAD_IS_ERROR (rc)) {
        //     DYAD_LOG_ERROR (ctx, "Could not create UCP endpoint");
        //     goto dtl_ucx_establish_connection_region_finish;
        // }
        if (dtl_handle->debug) {
            ucp_ep_print_info (dtl_handle->ep, stderr);
        }
        rc = DYAD_RC_OK;
    } else if (comm_mode == DYAD_COMM_RECV) {
        DYAD_LOG_INFO (ctx,
                       "No explicit connection establishment needed for UCX "
                       "receiver\n");
        rc = DYAD_RC_OK;
    } else {
        DYAD_LOG_ERROR (ctx, "Invalid communication mode: %d\n", comm_mode);
        // TODO create new RC for this
        rc = DYAD_RC_BAD_COMM_MODE;
    }
dtl_ucx_establish_connection_region_finish:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_send (const dyad_ctx_t* ctx, void* buf, size_t buflen)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    ucs_status_ptr_t stat_ptr;
    ucs_status_t status = UCS_OK;
    stat_ptr = ucx_send_no_wait (ctx, false, buf, buflen);
    DYAD_LOG_INFO (ctx, "Processing UCP send request\n");
    status = dyad_ucx_request_wait (ctx, stat_ptr);
    if (status != UCS_OK) {
        DYAD_LOG_ERROR (ctx,
                      "UCP Tag Send failed (status = %d)!\n",
                      (int)status);
        rc = DYAD_RC_UCXCOMM_FAIL;
        goto dtl_ucx_send_region_finish;
    }
    DYAD_LOG_INFO (ctx, "Data send with UCP succeeded\n");
    rc = DYAD_RC_OK;
dtl_ucx_send_region_finish:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_recv (const dyad_ctx_t* ctx, void** buf, size_t* buflen)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
//    ucs_status_ptr_t stat_ptr = NULL;
//    ucs_status_t status = UCS_OK;
//    // Wait on the recv operation to complete
//    stat_ptr = ucx_recv_no_wait (ctx, false, buf, buflen);
//    DYAD_LOG_INFO (ctx, "Wait for UCP recv operation to complete\n");
//    status = dyad_ucx_request_wait (ctx, stat_ptr);
//    // If the recv operation failed, log an error, free the data buffer,
//    // and set the buffer pointer to NULL
//    if (UCX_STATUS_FAIL (status)) {
//        DYAD_LOG_ERROR (ctx, "UCX recv failed!\n");
//        rc = DYAD_RC_UCXCOMM_FAIL;
//        goto dtl_ucx_recv_region_finish;
//    }
//    DYAD_LOG_INFO (ctx, "Data receive using UCX is successful\n");
//    DYAD_LOG_INFO (ctx, "Received %lu bytes from producer\n", *buflen);
    *buflen = ctx->dtl_handle->private_dtl.ucx_dtl_handle->max_transfer_size;
    rc = DYAD_RC_OK;
dtl_ucx_recv_region_finish:;
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_close_connection (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START();
    dyad_rc_t rc = DYAD_RC_OK;
    dyad_dtl_ucx_t* dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    dyad_dtl_comm_mode_t comm_mode = dtl_handle->comm_mode;
    if (comm_mode == DYAD_COMM_SEND) {
        if (dtl_handle != NULL) {
            // TODO replace this code (either here or elsewhere) with LRU eviction
            // rc = ucx_disconnect (ctx, dtl_handle->ucx_worker, dtl_handle->ep);
            // if (DYAD_IS_ERROR (rc)) {
            //     DYAD_LOG_ERROR (ctx,
            //                   "Could not successfully close Endpoint! However, endpoint was "
            //                   "released.");
            // }
            dtl_handle->ep = NULL;
            // Sender doesn't have a consumer address at this time
            // So, free the consumer address when closing the connection
            // NOTE: currently removing the deallocation of the remote address
            //       because it is still in use by the endpoint cache
            // if (dtl_handle->remote_address != NULL) {
            //     free (dtl_handle->remote_address);
            dtl_handle->remote_address = NULL;
            dtl_handle->remote_addr_len = 0;
            // }
            dtl_handle->comm_tag = 0;
        }
        DYAD_LOG_INFO (ctx, "UCP endpoint close successful\n");
        rc = DYAD_RC_OK;
    } else if (comm_mode == DYAD_COMM_RECV) {
        // Since we're using tag send/recv, there's no need
        // to explicitly close the connection. So, all we're
        // doing here is setting the tag back to 0 (which cannot
        // be valid for DYAD because DYAD won't send a file from
        // one node to the same node).
        dtl_handle->comm_tag = 0;
        rc = DYAD_RC_OK;
    } else {
        DYAD_LOG_ERROR (ctx,
                      "Somehow, an invalid comm mode reached "
                      "'close_connection'\n");
        // TODO create new RC for this case
        rc = DYAD_RC_BAD_COMM_MODE;
    }
    DYAD_C_FUNCTION_END();
    return rc;
}

dyad_rc_t dyad_dtl_ucx_finalize (const dyad_ctx_t* ctx)
{
    DYAD_C_FUNCTION_START();
    dyad_dtl_ucx_t* dtl_handle = NULL;
    dyad_rc_t rc = DYAD_RC_OK;
    if (ctx->dtl_handle == NULL || ctx->dtl_handle->private_dtl.ucx_dtl_handle == NULL) {
        rc = DYAD_RC_OK;
        goto dtl_ucx_finalize_region_finish;
    }
    dtl_handle = ctx->dtl_handle->private_dtl.ucx_dtl_handle;
    DYAD_LOG_INFO (ctx, "Finalizing UCX DTL\n");
    if (dtl_handle->ep != NULL) {
        dyad_dtl_ucx_close_connection (ctx);
        dtl_handle->ep = NULL;
    }
    if (dtl_handle->ep_cache != NULL) {
        dyad_ucx_ep_cache_finalize (ctx, &(dtl_handle->ep_cache), dtl_handle->ucx_worker);
        dtl_handle->ep_cache = NULL;
    }
    // Release consumer address if not already released
    if (dtl_handle->local_address != NULL) {
        ucp_worker_release_address (dtl_handle->ucx_worker, dtl_handle->local_address);
        dtl_handle->local_address = NULL;
    }
    // Free memory buffer if not already freed
    if (dtl_handle->mem_handle != NULL) {
        ucx_free_buffer (ctx,
                         dtl_handle->ucx_ctx,
                         dtl_handle->mem_handle,
                         &(dtl_handle->net_buf));
    }
    // Release worker if not already released
    if (dtl_handle->ucx_worker != NULL) {
        ucp_worker_destroy (dtl_handle->ucx_worker);
        dtl_handle->ucx_worker = NULL;
    }
    // Release context if not already released
    if (dtl_handle->ucx_ctx != NULL) {
        ucp_cleanup (dtl_handle->ucx_ctx);
        dtl_handle->ucx_ctx = NULL;
    }
    // Flux handle should be released by the
    // DYAD context, so it is not released here
    dtl_handle->h = NULL;
    // Free the handle and set to NULL to prevent double free
    free (dtl_handle);
    ctx->dtl_handle->private_dtl.ucx_dtl_handle = NULL;
    rc = DYAD_RC_OK;
dtl_ucx_finalize_region_finish:;
    DYAD_C_FUNCTION_END();
    return rc;
}
