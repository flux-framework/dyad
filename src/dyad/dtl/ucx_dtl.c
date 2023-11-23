#include "ucx_dtl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"

extern const base64_maps_t base64_maps_rfc4648;

// Tag mask for UCX Tag send/recv
#define DYAD_UCX_TAG_MASK UINT64_MAX

// Macro function used to simplify checking the status
// of UCX operations
#define UCX_STATUS_FAIL(status) (status != UCS_OK)

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
    dyad_ucx_request_t* real_request = NULL;
    real_request = (dyad_ucx_request_t*)request;
    real_request->completed = 0;
}

// Define a function that ucp_tag_msg_recv_nbx will use
// as a callback to signal the completion of the async receive
#if UCP_API_VERSION >= UCP_VERSION(1, 10)
static void dyad_recv_callback (void* request,
                                ucs_status_t status,
                                const ucp_tag_recv_info_t* tag_info,
                                void* user_data)
#else
static void dyad_recv_callback (void* request,
                                ucs_status_t status,
                                ucp_tag_recv_info_t* tag_info)
#endif
{
    dyad_ucx_request_t* real_request = NULL;
    real_request = (dyad_ucx_request_t*)request;
    real_request->completed = 1;
}

#if UCP_API_VERSION >= UCP_VERSION(1, 10)
static void dyad_send_callback (void* req, ucs_status_t status, void* ctx)
#else
static void dyad_send_callback (void* req, ucs_status_t status)
#endif
{
    dyad_ucx_request_t* real_req = (dyad_ucx_request_t*)req;
    real_req->completed = 1;
}

void dyad_ucx_ep_err_handler (void* arg, ucp_ep_h ep, ucs_status_t status)
{
    flux_t* h = (flux_t*)arg;
    FLUX_LOG_ERR (h, "An error occured on the UCP endpoint (status = %d)\n", status);
}

// Simple function used to wait on the async receive
static ucs_status_t dyad_ucx_request_wait (dyad_dtl_ucx_t* dtl_handle,
                                           dyad_ucx_request_t* request)
{
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
            ucp_worker_progress (dtl_handle->ucx_worker);
            // usleep(100);
            // Get the final status of the communication operation
            final_request_status = ucp_request_check_status (request);
        } while (final_request_status == UCS_INPROGRESS);
        // Free and deallocate the request object
        ucp_request_free (request);
        return final_request_status;
    }
    // If 'request' is actually a UCX error, this means the communication
    // operation immediately failed. In that case, we simply grab the
    // 'ucs_status_t' object for the error.
    else if (UCS_PTR_IS_ERR (request)) {
        return UCS_PTR_STATUS (request);
    }
    // If 'request' is neither a request handle nor an error, then
    // the communication operation immediately completed successfully.
    // So, we simply set the status to UCS_OK
    return UCS_OK;
}

static inline dyad_rc_t dyad_dtl_ucx_finalize_impl (dyad_dtl_ucx_t** dtl_handle)
{
}

dyad_rc_t dyad_dtl_ucx_init (dyad_dtl_t* self,
                             dyad_dtl_mode_t mode,
                             flux_t* h,
                             bool debug)
{
    ucp_params_t ucx_params;
    ucp_worker_params_t worker_params;
    ucp_config_t* config;
    ucs_status_t status;
    ucp_worker_attr_t worker_attrs;
    dyad_dtl_ucx_t* dtl_handle = NULL;

    self->private.ucx_dtl_handle = malloc (sizeof (struct dyad_dtl_ucx));
    if (self->private.ucx_dtl_handle == NULL) {
        FLUX_LOG_ERR (h, "Could not allocate UCX DTL context\n");
        return DYAD_RC_SYSFAIL;
    }
    dtl_handle = self->private.ucx_dtl_handle;
    // Allocation/Freeing of the Flux handle should be
    // handled by the DYAD context
    dtl_handle->h = h;
    dtl_handle->debug = debug;
    dtl_handle->ucx_ctx = NULL;
    dtl_handle->ucx_worker = NULL;
    dtl_handle->ep = NULL;
    dtl_handle->curr_comm_mode = DYAD_COMM_NONE;
    dtl_handle->consumer_address = NULL;
    dtl_handle->addr_len = 0;
    dtl_handle->comm_tag = 0;

    // Read the UCX configuration
    FLUX_LOG_INFO (dtl_handle->h, "Reading UCP config\n");
    status = ucp_config_read (NULL, NULL, &config);
    if (UCX_STATUS_FAIL (status)) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not read the UCX config\n");
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
    ucx_params.field_mask = UCP_PARAM_FIELD_FEATURES | UCP_PARAM_FIELD_REQUEST_SIZE
                            | UCP_PARAM_FIELD_REQUEST_INIT;
    ucx_params.features = UCP_FEATURE_TAG |
                          // UCP_FEATURE_RMA |
                          UCP_FEATURE_WAKEUP;
    ucx_params.request_size = sizeof (struct ucx_request);
    ucx_params.request_init = dyad_ucx_request_init;

    // Initialize UCX
    FLUX_LOG_INFO (dtl_handle->h, "Initializing UCP\n");
    status = ucp_init (&ucx_params, config, &dtl_handle->ucx_ctx);

    // If in debug mode, print the configuration of UCX to stderr
    if (debug) {
        ucp_config_print (config, stderr, "UCX Configuration", UCS_CONFIG_PRINT_CONFIG);
    }
    // Release the config
    ucp_config_release (config);
    // Log an error if UCX initialization failed
    if (UCX_STATUS_FAIL (status)) {
        FLUX_LOG_ERR (h, "ucp_init failed (status = %d)\n", status);
        goto error;
    }

    // Define the settings for the UCX worker (i.e., progress engine)
    //
    // The settings enabled are:
    //   * Single-threaded mode (TODO look into multi-threading support)
    //   * Restricting wakeup events to only include Tag-matching recv events
    worker_params.field_mask =
        UCP_WORKER_PARAM_FIELD_THREAD_MODE | UCP_WORKER_PARAM_FIELD_EVENTS;
    // TODO look into multi-threading support
    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
    worker_params.events = UCP_WAKEUP_TAG_RECV;

    // Create the worker and log an error if that fails
    FLUX_LOG_INFO (dtl_handle->h, "Creating UCP worker\n");
    status = ucp_worker_create (dtl_handle->ucx_ctx,
                                &worker_params,
                                &(dtl_handle->ucx_worker));
    if (UCX_STATUS_FAIL (status)) {
        FLUX_LOG_ERR (dtl_handle->h, "ucp_worker_create failed (status = %d)!\n", status);
        goto error;
    }

    // Query the worker for its address
    worker_attrs.field_mask = UCP_WORKER_ATTR_FIELD_ADDRESS;
    FLUX_LOG_INFO (dtl_handle->h, "Get address of UCP worker\n");
    status = ucp_worker_query (dtl_handle->ucx_worker, &worker_attrs);
    if (UCX_STATUS_FAIL (status)) {
        FLUX_LOG_ERR (h, "Cannot get UCX worker address (status = %d)!\n", status);
        goto error;
    }
    dtl_handle->consumer_address = worker_attrs.address;
    dtl_handle->addr_len = worker_attrs.address_length;

    self->rpc_pack = dyad_dtl_ucx_rpc_pack;
    self->rpc_unpack = dyad_dtl_ucx_rpc_unpack;
    self->rpc_respond = dyad_dtl_ucx_rpc_respond;
    self->rpc_recv_response = dyad_dtl_ucx_rpc_recv_response;
    self->establish_connection = dyad_dtl_ucx_establish_connection;
    self->send = dyad_dtl_ucx_send;
    self->recv = dyad_dtl_ucx_recv;
    self->close_connection = dyad_dtl_ucx_close_connection;

    return DYAD_RC_OK;

error:;
    // If an error occured, finalize the DTL handle and
    // return a failing error code
    dyad_dtl_ucx_finalize (&self);
    return DYAD_RC_UCXINIT_FAIL;
}

dyad_rc_t dyad_dtl_ucx_rpc_pack (dyad_dtl_t* restrict self,
                                 const char* restrict upath,
                                 uint32_t producer_rank,
                                 json_t** restrict packed_obj)
{
    size_t enc_len = 0;
    char* enc_buf = NULL;
    ssize_t enc_size = 0;
    dyad_dtl_ucx_t* dtl_handle = self->private.ucx_dtl_handle;
    if (dtl_handle->consumer_address == NULL) {
        // TODO log error
        return DYAD_RC_BADPACK;
    }
    FLUX_LOG_INFO (dtl_handle->h, "Encode UCP address using base64\n");
    enc_len = base64_encoded_length (dtl_handle->addr_len);
    // Add 1 to encoded length because the encoded buffer will be
    // packed as if it is a string
    enc_buf = malloc (enc_len + 1);
    if (enc_buf == NULL) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not allocate buffer for packed address\n");
        return DYAD_RC_SYSFAIL;
    }
    // consumer_address is casted to const char* to avoid warnings
    // This is valid because it is a pointer to an opaque struct,
    // so the cast can be treated like a void*->char* cast.
    enc_size = base64_encode_using_maps (&base64_maps_rfc4648,
                                         enc_buf,
                                         enc_len + 1,
                                         (const char*)dtl_handle->consumer_address,
                                         dtl_handle->addr_len);
    if (enc_size < 0) {
        // TODO log error
        free (enc_buf);
        return DYAD_RC_BADPACK;
    }
    FLUX_LOG_INFO (dtl_handle->h, "Creating UCP tag for tag matching\n");
    // Because we're using tag-matching send/recv for communication,
    // there's no need to do any real connection establishment here.
    // Instead, we use this function to create the tag that will be
    // used for the upcoming communication.
    uint32_t consumer_rank = 0;
    if (flux_get_rank (dtl_handle->h, &consumer_rank) < 0) {
        FLUX_LOG_ERR (dtl_handle->h, "Cannot get consumer rank\n");
        return DYAD_RC_FLUXFAIL;
    }
    // The tag is a 64 bit unsigned integer consisting of the
    // 32-bit rank of the producer followed by the 32-bit rank
    // of the consumer
    dtl_handle->comm_tag = ((uint64_t)producer_rank << 32) | (uint64_t)consumer_rank;
    // Use Jansson to pack the tag and UCX address into
    // the payload to be sent via RPC to the producer plugin
    FLUX_LOG_INFO (dtl_handle->h, "Packing RPC payload for UCX DTL\n");
    *packed_obj = json_pack ("{s:s, s:i, s:i, s:s%}",
                             "upath",
                             upath,
                             "tag_prod",
                             (int)producer_rank,
                             "tag_cons",
                             (int)consumer_rank,
                             "ucx_addr",
                             enc_buf,
                             enc_len);
    free (enc_buf);
    // If the packing failed, log an error
    if (*packed_obj == NULL) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not pack upath and UCX address for RPC\n");
        return DYAD_RC_BADPACK;
    }
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_rpc_unpack (dyad_dtl_t* self, const flux_msg_t* msg, char** upath)
{
    char* enc_addr = NULL;
    size_t enc_addr_len = 0;
    int errcode = 0;
    uint32_t tag_prod = 0;
    uint32_t tag_cons = 0;
    ssize_t decoded_len = 0;
    dyad_dtl_ucx_t* dtl_handle = self->private.ucx_dtl_handle;
    FLUX_LOG_INFO (dtl_handle->h, "Unpacking RPC payload\n");
    errcode = flux_request_unpack (msg,
                                   NULL,
                                   "{s:s, s:i, s:i, s:s%}",
                                   "upath",
                                   upath,
                                   "tag_prod",
                                   &tag_prod,
                                   "tag_cons",
                                   &tag_cons,
                                   "ucx_addr",
                                   &enc_addr,
                                   &enc_addr_len);
    if (errcode < 0) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not unpack Flux message from consumer!\n");
        return DYAD_RC_BADUNPACK;
    }
    dtl_handle->comm_tag = ((uint64_t)tag_prod << 32) | (uint64_t)tag_cons;
    FLUX_LOG_INFO (dtl_handle->h, "Obtained upath from RPC payload: %s\n", upath);
    FLUX_LOG_INFO (dtl_handle->h,
                   "Obtained UCP tag from RPC payload: %lu\n",
                   dtl_handle->comm_tag);
    FLUX_LOG_INFO (dtl_handle->h, "Decoding consumer UCP address using base64\n");
    dtl_handle->addr_len = base64_decoded_length (enc_addr_len);
    dtl_handle->consumer_address = (ucp_address_t*)malloc (dtl_handle->addr_len);
    if (dtl_handle->consumer_address == NULL) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not allocate memory for consumer address");
        return DYAD_RC_SYSFAIL;
    }
    decoded_len = base64_decode_using_maps (&base64_maps_rfc4648,
                                            (char*)dtl_handle->consumer_address,
                                            dtl_handle->addr_len,
                                            enc_addr,
                                            enc_addr_len);
    if (decoded_len < 0) {
        // TODO log error
        free (dtl_handle->consumer_address);
        dtl_handle->consumer_address = NULL;
        dtl_handle->addr_len = 0;
        return DYAD_RC_BAD_B64DECODE;
    }
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_rpc_respond (dyad_dtl_t* self, const flux_msg_t* orig_msg)
{
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_rpc_recv_response (dyad_dtl_t* self, flux_future_t* f)
{
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_establish_connection (dyad_dtl_t* self,
                                             dyad_dtl_comm_mode_t comm_mode)
{
    ucp_ep_params_t params;
    ucs_status_t status = UCS_OK;
    dyad_dtl_ucx_t* dtl_handle = self->private.ucx_dtl_handle;
    dtl_handle->curr_comm_mode = comm_mode;
    if (comm_mode == DYAD_COMM_SEND) {
        params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS
                            | UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE
                            | UCP_EP_PARAM_FIELD_ERR_HANDLER;
        params.address = dtl_handle->consumer_address;
        params.err_mode = UCP_ERR_HANDLING_MODE_PEER;
        params.err_handler.cb = dyad_ucx_ep_err_handler;
        params.err_handler.arg = (void*)dtl_handle->h;
        FLUX_LOG_INFO (dtl_handle->h,
                       "Create UCP endpoint for communication with consumer\n");
        status = ucp_ep_create (dtl_handle->ucx_worker, &params, &dtl_handle->ep);
        if (status != UCS_OK) {
            FLUX_LOG_ERR (dtl_handle->h,
                          "ucp_ep_create failed with status %d\n",
                          (int)status);
            return DYAD_RC_UCXCOMM_FAIL;
        }
        if (dtl_handle->debug) {
            ucp_ep_print_info (dtl_handle->ep, stderr);
        }
        return DYAD_RC_OK;
    } else if (comm_mode == DYAD_COMM_RECV) {
        FLUX_LOG_INFO (dtl_handle->h,
                       "No explicit connection establishment needed for UCX "
                       "receiver\n");
        return DYAD_RC_OK;
    } else {
        FLUX_LOG_ERR (dtl_handle->h, "Invalid communication mode: %d\n", comm_mode);
        // TODO create new RC for this
        return DYAD_RC_BAD_COMM_MODE;
    }
}

dyad_rc_t dyad_dtl_ucx_send (dyad_dtl_t* self, void* buf, size_t buflen)
{
    ucs_status_ptr_t stat_ptr;
    ucs_status_t status = UCS_OK;
    dyad_ucx_request_t* req = NULL;
    dyad_dtl_ucx_t* dtl_handle = self->private.ucx_dtl_handle;
    if (dtl_handle->ep == NULL) {
        FLUX_LOG_INFO (dtl_handle->h,
                       "UCP endpoint was not created prior to invoking "
                       "send!\n");
        return DYAD_RC_UCXCOMM_FAIL;
    }
    // ucp_tag_send_sync_nbx is the prefered version of this send since UCX 1.9
    // However, some systems (e.g., Lassen) may have an older verison
    // This conditional compilation will use ucp_tag_send_sync_nbx if using
    // UCX 1.9+, and it will use the deprecated ucp_tag_send_sync_nb if using
    // UCX < 1.9.
#if UCP_API_VERSION >= UCP_VERSION(1, 10)
    ucp_request_param_t params;
    params.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK;
    params.cb.send = dyad_send_callback;
    FLUX_LOG_INFO (dtl_handle->h, "Sending data to consumer with ucp_tag_send_nbx\n");
    stat_ptr =
        ucp_tag_send_nbx (dtl_handle->ep, buf, buflen, dtl_handle->comm_tag, &params);
#else
    FLUX_LOG_INFO (dtl_handle->h,
                   "Sending %lu bytes of data to consumer with "
                   "ucp_tag_send_nb\n",
                   buflen);
    stat_ptr = ucp_tag_send_nb (dtl_handle->ep,
                                buf,
                                buflen,
                                UCP_DATATYPE_CONTIG,
                                dtl_handle->comm_tag,
                                dyad_send_callback);
#endif
    FLUX_LOG_INFO (dtl_handle->h, "Processing UCP send request\n");
    status = dyad_ucx_request_wait (dtl_handle, stat_ptr);
    if (status != UCS_OK) {
        FLUX_LOG_ERR (dtl_handle->h, "UCP Tag Send failed (status = %d)!\n", (int)status);
        return DYAD_RC_UCXCOMM_FAIL;
    }
    FLUX_LOG_INFO (dtl_handle->h, "Data send with UCP succeeded\n");
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_recv (dyad_dtl_t* self, void** buf, size_t* buflen)
{
    ucs_status_t status = UCS_OK;
    ucp_tag_message_h msg = NULL;
    ucp_tag_recv_info_t msg_info;
    dyad_ucx_request_t* req = NULL;
    dyad_dtl_ucx_t* dtl_handle = self->private.ucx_dtl_handle;
    FLUX_LOG_INFO (dtl_handle->h, "Poll UCP for incoming data\n");
    // TODO replace this loop with a resiliency response over RPC
    do {
        ucp_worker_progress (dtl_handle->ucx_worker);
        msg = ucp_tag_probe_nb (dtl_handle->ucx_worker,
                                dtl_handle->comm_tag,
                                DYAD_UCX_TAG_MASK,
                                1,  // Remove the message from UCP tracking
                                // Requires calling ucp_tag_msg_recv_nb
                                // with the ucp_tag_message_h to retrieve message
                                &msg_info);
    } while (msg == NULL);
    // TODO: This version of the polling code is not supposed to spin-lock,
    // unlike the code above. Currently, it does not work. Once it starts
    // working, we can replace the code above with a version of this code.
    //
    // while (true)
    // {
    //     // Probe the tag recv event at the top
    //     // of the worker's queue
    //     FLUX_LOG_INFO (dtl_handle->h, "Probe UCP worker with tag %lu\n",
    //     dtl_handle->comm_tag); msg = ucp_tag_probe_nb(
    //         dtl_handle->ucx_worker,
    //         dtl_handle->comm_tag,
    //         DYAD_UCX_TAG_MASK,
    //         1, // Remove the message from UCP tracking
    //         // Requires calling ucp_tag_msg_recv_nb
    //         // with the ucp_tag_message_h to retrieve message
    //         &msg_info
    //     );
    //     // If data has arrived from the producer plugin,
    //     // break the loop
    //     if (msg != NULL)
    //     {
    //         FLUX_LOG_INFO (dtl_handle->h, "Data has arrived, so end
    //         polling\n"); break;
    //     }
    //     // If data has not arrived, check if there are
    //     // any other events in the worker's queue.
    //     // If so, start the loop over to handle the next event
    //     else if (ucp_worker_progress(dtl_handle->ucx_worker))
    //     {
    //         FLUX_LOG_INFO (dtl_handle->h, "Progressed UCP worker to check if
    //         any other UCP events are available\n"); continue;
    //     }
    //     // No other events are queued. So, we will wait on new
    //     // events to come in. By using 'ucp_worker_wait' for this,
    //     // we let the OS do other work in the meantime (no spin locking).
    //     FLUX_LOG_INFO (dtl_handle->h, "Launch pre-emptable wait until UCP
    //     worker gets new events\n"); status =
    //     ucp_worker_wait(dtl_handle->ucx_worker);
    //     // If the wait fails, log an error
    //     if (UCX_STATUS_FAIL(status))
    //     {
    //         FLUX_LOG_ERR (dtl_handle->h, "Could not wait on the message from
    //         the producer plugin\n"); return DYAD_RC_UCXWAIT_FAIL;
    //     }
    // }
    // The metadata retrived from the probed tag recv event contains
    // the size of the data to be sent.
    // So, use that size to allocate a buffer
    FLUX_LOG_INFO (dtl_handle->h,
                   "Got message with tag %lu and size %lu\n",
                   msg_info.sender_tag,
                   msg_info.length);
    *buflen = msg_info.length;
    *buf = malloc (*buflen);
    // If allocation fails, log an error
    if (*buf == NULL) {
        FLUX_LOG_ERR (dtl_handle->h, "Could not allocate memory for file\n");
        return DYAD_RC_SYSFAIL;
    }
    FLUX_LOG_INFO (dtl_handle->h, "Receive data using async UCX operation\n");
#if UCP_API_VERSION >= UCP_VERSION(1, 10)
    // Define the settings for the recv operation
    //
    // The settings enabled are:
    //   * Define callback for the recv because it is async
    //   * Restrict memory buffers to host-only since we aren't directly
    //     dealing with GPU memory
    ucp_request_param_t recv_params;
    // TODO consider enabling UCP_OP_ATTR_FIELD_MEMH to speedup
    // the recv operation if using RMA behind the scenes
    recv_params.op_attr_mask = UCP_OP_ATTR_FIELD_CALLBACK | UCP_OP_ATTR_FIELD_MEMORY_TYPE;
    recv_params.cb.recv = dyad_recv_callback;
    // Constraining to Host memory (as opposed to GPU memory)
    // allows UCX to potentially perform some optimizations
    recv_params.memory_type = UCS_MEMORY_TYPE_HOST;
    // Perform the async recv operation using the probed tag recv event
    req = ucp_tag_msg_recv_nbx (dtl_handle->ucx_worker, *buf, *buflen, msg, &recv_params);
#else
    req = ucp_tag_msg_recv_nb (dtl_handle->ucx_worker,
                               *buf,
                               *buflen,
                               UCP_DATATYPE_CONTIG,
                               msg,
                               dyad_recv_callback);
#endif
    // Wait on the recv operation to complete
    FLUX_LOG_INFO (dtl_handle->h, "Wait for UCP recv operation to complete\n");
    status = dyad_ucx_request_wait (dtl_handle, req);
    // If the recv operation failed, log an error, free the data buffer,
    // and set the buffer pointer to NULL
    if (UCX_STATUS_FAIL (status)) {
        FLUX_LOG_ERR (dtl_handle->h, "UCX recv failed!\n");
        free (*buf);
        *buf = NULL;
        return DYAD_RC_UCXCOMM_FAIL;
    }
    FLUX_LOG_INFO (dtl_handle->h, "Data receive using UCX is successful\n");
    FLUX_LOG_INFO (dtl_handle->h, "Received %lu bytes from producer\n", *buflen);
    return DYAD_RC_OK;
}

dyad_rc_t dyad_dtl_ucx_close_connection (dyad_dtl_t* self)
{
    ucs_status_t status = UCS_OK;
    ucs_status_ptr_t stat_ptr;
    dyad_dtl_ucx_t* dtl_handle = self->private.ucx_dtl_handle;
    if (dtl_handle->curr_comm_mode == DYAD_COMM_SEND) {
        if (dtl_handle != NULL) {
            if (dtl_handle->ep != NULL) {
                // ucp_tag_send_sync_nbx is the prefered version of this send
                // since UCX 1.9 However, some systems (e.g., Lassen) may have
                // an older verison This conditional compilation will use
                // ucp_tag_send_sync_nbx if using UCX 1.9+, and it will use the
                // deprecated ucp_tag_send_sync_nb if using UCX < 1.9.
                FLUX_LOG_INFO (dtl_handle->h, "Start async closing of UCP endpoint\n");
#if UCP_API_VERSION >= UCP_VERSION(1, 10)
                ucp_request_param_t close_params;
                close_params.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
                close_params.flags = UCP_EP_CLOSE_FLAG_FORCE;
                stat_ptr = ucp_ep_close_nbx (dtl_handle->ep, &close_params);
#else
                // TODO change to FORCE if we decide to enable err handleing
                // mode
                stat_ptr = ucp_ep_close_nb (dtl_handle->ep, UCP_EP_CLOSE_MODE_FORCE);
#endif
                FLUX_LOG_INFO (dtl_handle->h, "Wait for endpoint closing to finish\n");
                // Don't use dyad_ucx_request_wait here because ep_close behaves
                // differently than other UCX calls
                if (stat_ptr != NULL) {
                    // Endpoint close is in-progress.
                    // Wait until finished
                    if (UCS_PTR_IS_PTR (stat_ptr)) {
                        do {
                            ucp_worker_progress (dtl_handle->ucx_worker);
                            status = ucp_request_check_status (stat_ptr);
                        } while (status == UCS_INPROGRESS);
                        ucp_request_free (stat_ptr);
                    }
                    // An error occurred during endpoint closure
                    // However, the endpoint can no longer be used
                    // Get the status code for reporting
                    else {
                        status = UCS_PTR_STATUS (stat_ptr);
                    }
                    if (status != UCS_OK) {
                        FLUX_LOG_ERR (dtl_handle->h,
                                      "Could not successfully close Endpoint "
                                      "(status = %d)! However, endpoint was "
                                      "released.\n",
                                      status);
                    }
                }
                dtl_handle->ep = NULL;
            }
            // Sender doesn't have a consumer address at this time
            // So, free the consumer address when closing the connection
            if (dtl_handle->consumer_address != NULL) {
                free (dtl_handle->consumer_address);
                dtl_handle->consumer_address = NULL;
                dtl_handle->addr_len = 0;
            }
            dtl_handle->comm_tag = 0;
        }
        FLUX_LOG_INFO (dtl_handle->h, "UCP endpoint close successful\n");
        return DYAD_RC_OK;
    } else if (dtl_handle->curr_comm_mode == DYAD_COMM_RECV) {
        // Since we're using tag send/recv, there's no need
        // to explicitly close the connection. So, all we're
        // doing here is setting the tag back to 0 (which cannot
        // be valid for DYAD because DYAD won't send a file from
        // one node to the same node).
        dtl_handle->comm_tag = 0;
        return DYAD_RC_OK;
    } else {
        FLUX_LOG_ERR (dtl_handle->h,
                      "Somehow, an invalid comm mode reached "
                      "'close_connection'\n");
        // TODO create new RC for this case
        return DYAD_RC_BAD_COMM_MODE;
    }
}

dyad_rc_t dyad_dtl_ucx_finalize (dyad_dtl_t** self)
{
    dyad_dtl_ucx_t* dtl_handle = NULL;
    dyad_rc_t rc = DYAD_RC_OK;
    if (self == NULL || *self == NULL || (*self)->private.ucx_dtl_handle == NULL) {
        return DYAD_RC_OK;
    }
    dtl_handle = (*self)->private.ucx_dtl_handle;
    FLUX_LOG_INFO (dtl_handle->h, "Finalizing UCX DTL\n");
    if (dtl_handle->ep != NULL) {
        dyad_dtl_ucx_close_connection (*self);
        dtl_handle->ep = NULL;
    }
    // Release consumer address if not already released
    if (dtl_handle->consumer_address != NULL) {
        ucp_worker_release_address (dtl_handle->ucx_worker, dtl_handle->consumer_address);
        dtl_handle->consumer_address = NULL;
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
    (*self)->private.ucx_dtl_handle = NULL;
    return DYAD_RC_OK;
}
