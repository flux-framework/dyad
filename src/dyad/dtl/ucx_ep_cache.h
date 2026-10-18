#ifndef DYAD_DTL_UCX_EP_CACHE_H
#define DYAD_DTL_UCX_EP_CACHE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

// clang-format off
#include <flux/core.h>
#include <ucp/api/ucp.h>

#include <dyad/common/dyad_rc.h>
#include <dyad/common/dyad_structures_int.h>
#include <dyad/dtl/ucx_dtl.h>
#include <ucp/api/ucp.h>
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tests whether a UCX operation status indicates failure.
 *
 * @details
 * UCX operations return @c ucs_status_t values where @c UCS_OK (0)
 * indicates success and any other value indicates an error or in-progress
 * state. This macro simplifies the common pattern of checking for
 * non-@c UCS_OK status after UCX API calls.
 *
 * @note This macro does not distinguish between hard errors and
 *       @c UCS_INPROGRESS — both evaluate to @c true. For operations
 *       that may return @c UCS_INPROGRESS as a normal in-progress
 *       indicator (e.g. @c ucp_request_check_status()), check
 *       @c UCS_INPROGRESS explicitly rather than using this macro.
 *
 * @param status A @c ucs_status_t value returned by a UCX API call.
 * @return Non-zero (true) if @p status is not @c UCS_OK, zero (false)
 *         if @p status is @c UCS_OK.
 */
#define UCX_STATUS_FAIL(status) (status != UCS_OK)

/**
 * @brief Creates a UCX endpoint to a remote worker.
 *
 * @details
 * Creates a @c ucp_ep_h to the remote worker identified by @p addr
 * via @c ucp_ep_create(). The endpoint is initialized with only
 * @c UCP_EP_PARAM_FIELD_REMOTE_ADDRESS set — no error handler is
 * registered (see TODO in @c dyad_ucx_ep_err_handler()).
 *
 * Validates the created endpoint — if @c ucp_ep_create() succeeds but
 * returns a @c NULL endpoint, the function treats this as a failure
 * and returns @c DYAD_RC_UCXCOMM_FAIL.
 *
 * @note This function is used both during the warmup loopback connection
 *       in @c ucx_warmup() and during real connection establishment in
 *       @c dyad_ucx_ep_cache_insert(). In the normal transfer path,
 *       @c dyad_dtl_ucx_establish_connection() uses the endpoint cache
 *       and calls this function indirectly via @c dyad_ucx_ep_cache_insert()
 *       only when no cached endpoint exists for the consumer.
 *
 * @param[in]  ctx    DYAD context. Used for logging.
 * @param[in]  worker UCX worker on which to create the endpoint.
 * @param[in]  addr   UCX address of the remote worker to connect to.
 * @param[out] ep     Set to the created @c ucp_ep_h on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK            Endpoint created successfully.
 * @retval DYAD_RC_UCXCOMM_FAIL  @c ucp_ep_create() failed or returned
 *                               a @c NULL endpoint.
 */
dyad_rc_t ucx_connect (const dyad_ctx_t *ctx,
                       ucp_worker_h worker,
                       const ucp_address_t *addr,
                       ucp_ep_h *ep);

/**
 * @brief Closes a UCX endpoint and waits for the closure to complete.
 *
 * @details
 * Initiates a non-blocking endpoint close via @c ucp_ep_close_nbx()
 * (UCX >= 1.10) or @c ucp_ep_close_nb() (UCX < 1.10), both using
 * @c UCP_EP_CLOSE_FLAG_FORCE / @c UCP_EP_CLOSE_MODE_FORCE to forcefully
 * terminate the endpoint without waiting for in-flight operations to
 * complete. If @p ep is @c NULL the function is a no-op.
 *
 * Unlike other UCX non-blocking operations, endpoint close does not
 * use @c dyad_ucx_request_wait() because the close request behaves
 * differently from communication requests — it must be waited on by
 * calling @c ucp_worker_progress() directly rather than through the
 * standard request polling path. The wait loop is therefore inlined:
 *
 *  - If the returned @c stat_ptr is a request handle
 *    (@c UCS_PTR_IS_PTR), spins calling @c ucp_worker_progress() and
 *    @c ucp_request_check_status() until the status is no longer
 *    @c UCS_INPROGRESS, then frees the request via @c ucp_request_free().
 *  - If the returned @c stat_ptr encodes an error
 *    (@c UCS_PTR_IS_ERR), extracts the status code for reporting.
 *  - If @c stat_ptr is @c NULL, the close completed immediately.
 *
 * @note Force-close is used regardless of UCX version. If error handler
 *       mode is enabled in the future, the close mode may need to change
 *       to @c UCP_EP_CLOSE_MODE_FLUSH to allow in-flight operations to
 *       drain before closing (see TODO in source).
 *
 * @note This function is called by @c dyad_ucx_ep_cache_finalize()
 *       when evicting endpoints from the cache, and by @c ucx_warmup()
 *       after the loopback warmup transfer. It is @b not called during
 *       normal transfer close — @c dyad_dtl_ucx_close_connection() sets
 *       @c dtl_handle->ep to @c NULL and relies on the cache to manage
 *       the endpoint lifetime.
 *
 * @param[in] ctx    DYAD context. Used for logging.
 * @param[in] worker UCX worker needed to progress the close operation.
 * @param[in] ep     UCX endpoint to close. If @c NULL, the function
 *                   is a no-op and returns @c DYAD_RC_OK.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK         Endpoint closed successfully or was @c NULL.
 * @retval DYAD_RC_UCXEP_FAIL The endpoint close operation failed. The
 *                            endpoint can no longer be used regardless
 *                            of the return code.
 */
dyad_rc_t ucx_disconnect (const dyad_ctx_t *ctx, ucp_worker_h worker, ucp_ep_h ep);

/**
 * @brief Allocates and initializes the UCX endpoint cache.
 *
 * @details
 * Allocates a new @c cache_type (@c std::unordered_map<key_type, cache_entry>)
 * using @c new(std::nothrow) and stores a @c reinterpret_cast pointer
 * to it in @p *cache as an opaque @c ucx_ep_cache_h handle. Using
 * @c std::nothrow ensures that allocation failure returns @c nullptr
 * rather than throwing @c std::bad_alloc.
 *
 * Validates @p cache before allocation:
 * - If @p cache is @c nullptr, the caller passed an invalid output
 *   pointer and @c DYAD_RC_BADBUF is returned.
 * - If @p *cache is already non-@c nullptr, a cache is already present
 *   and overwriting it would leak the existing allocation, so
 *   @c DYAD_RC_BADBUF is returned.
 *
 * @param[in]  ctx   DYAD context. Used for logging.
 * @param[out] cache Must point to a @c nullptr on entry. Set to the
 *                   allocated cache handle on success.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      Cache allocated successfully.
 * @retval DYAD_RC_BADBUF  @p cache is @c nullptr or @p *cache is
 *                         already non-@c nullptr.
 * @retval DYAD_RC_SYSFAIL @c new(std::nothrow) failed to allocate
 *                         the cache.
 *
 * @todo Add an option to configure replacement strategy
 */
dyad_rc_t dyad_ucx_ep_cache_init (const dyad_ctx_t *ctx, ucx_ep_cache_h *cache);

/**
 * @brief Looks up a cached UCX endpoint by consumer connection key.
 *
 * @details
 * Searches the endpoint cache for an entry matching
 * @c ctx->dtl_handle->private_dtl.ucx_dtl_handle->consumer_conn_key
 * (@c pid << 32 | tag_cons). If found, sets @p *ep to the cached
 * @c ucp_ep_h and returns @c DYAD_RC_OK. If not found, sets @p *ep
 * to @c nullptr and returns @c DYAD_RC_NOTFOUND.
 *
 * @note The @p addr and @p addr_size parameters are accepted for
 *       interface consistency but are not used — the lookup is performed
 *       by @c consumer_conn_key rather than by raw address bytes, since
 *       @c ucp_address_t is an opaque struct that cannot be reliably
 *       compared byte-by-byte.
 *
 * @note All cache operations are wrapped in a @c try / @c catch(...)
 *       block to prevent C++ exceptions from propagating into the C
 *       calling code. Any exception is caught and returned as
 *       @c DYAD_RC_SYSFAIL.
 *
 * Validates @p ep before searching:
 * - If @p ep is @c nullptr, the caller passed an invalid output pointer
 *   and @c DYAD_RC_BADBUF is returned.
 * - If @p *ep is already non-@c nullptr, an endpoint is already present
 *   and overwriting it would leak the existing handle, so
 *   @c DYAD_RC_BADBUF is returned.
 *
 * @param[in]  ctx       DYAD context. The @c consumer_conn_key used as
 *                       the cache lookup key is read from the UCX DTL
 *                       internal state.
 * @param[in]  cache     Endpoint cache handle to search.
 * @param[in]  addr      Unused. Accepted for interface consistency.
 * @param[in]  addr_size Unused. Accepted for interface consistency.
 * @param[out] ep        Must point to a @c nullptr on entry. Set to the
 *                       cached @c ucp_ep_h on success, or @c nullptr
 *                       if not found.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK       A cached endpoint was found and returned.
 * @retval DYAD_RC_NOTFOUND No cached endpoint exists for the current
 *                          @c consumer_conn_key.
 * @retval DYAD_RC_BADBUF   @p ep is @c nullptr or @p *ep is already
 *                          non-@c nullptr.
 * @retval DYAD_RC_SYSFAIL  An unexpected C++ exception was thrown
 *                          during the cache lookup.
 */
dyad_rc_t dyad_ucx_ep_cache_find (const dyad_ctx_t *ctx,
                                  const ucx_ep_cache_h cache,
                                  const ucp_address_t *addr,
                                  const size_t addr_size,
                                  ucp_ep_h *ep);

/**
 * @brief Inserts a new UCX endpoint into the cache, creating it if needed.
 *
 * @details
 * Looks up @c consumer_conn_key in the cache. If an entry already exists,
 * returns @c DYAD_RC_OK immediately without creating a new endpoint —
 * this handles the case where @c dyad_dtl_ucx_establish_connection()
 * calls @c dyad_ucx_ep_cache_find() and @c dyad_ucx_ep_cache_insert()
 * in sequence and the entry was inserted between the two calls.
 *
 * If no entry exists, creates a new @c ucp_ep_h to @p *addr via
 * @c ucx_connect() and inserts it into the cache. On that successful
 * insertion, the cache takes ownership of the @c malloc()-allocated
 * address and sets @p *addr to @c NULL. The address remains valid until
 * the cached endpoint has been disconnected; this is required because
 * UCX endpoint creation is non-blocking and may still access it during
 * connection wireup. The new endpoint is also stored directly in
 * @c ctx->dtl_handle->private_dtl.ucx_dtl_handle->ep for immediate
 * use by the caller.
 *
 * @note All cache operations are wrapped in a @c try / @c catch(...)
 *       block to prevent C++ exceptions from propagating into the C
 *       calling code. Any exception is caught and returned as
 *       @c DYAD_RC_SYSFAIL.
 *
 * If an entry appears before insertion, the newly created endpoint is
 * disconnected, the existing endpoint is returned through the DTL state,
 * and @p *addr remains owned by the caller.
 *
 * @param[in] ctx       DYAD context. The @c consumer_conn_key and
 *                      @c ep fields of the UCX DTL internal state are
 *                      read and written.
 * @param[in] cache     Endpoint cache handle to insert into.
 * @param[in,out] addr  Address of the remote worker to connect to. Must
 *                      point to a non-@c NULL @c malloc()-allocated
 *                      address on entry. Set to @c NULL only when the
 *                      cache successfully takes ownership.
 * @param[in] addr_size Size of @p *addr in bytes. Retained for interface
 *                      consistency; endpoint creation does not require it.
 * @param[in] worker    UCX worker used to create the new endpoint via
 *                      @c ucx_connect().
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK       A cached entry already existed, or a new
 *                          endpoint was created and inserted successfully.
 * @retval DYAD_RC_SYSFAIL  An unexpected C++ exception was thrown during
 *                          the cache operation.
 * @retval other            Any error code returned by @c ucx_connect()
 *                          if endpoint creation failed.
 */
dyad_rc_t dyad_ucx_ep_cache_insert (const dyad_ctx_t *ctx,
                                    ucx_ep_cache_h cache,
                                    ucp_address_t **addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker);

/**
 * @brief Removes a single UCX endpoint from the cache and disconnects it.
 *
 * @details
 * Looks up @c consumer_conn_key in the cache and, if found, delegates
 * to @c cache_remove_impl() to disconnect the endpoint via
 * @c ucx_disconnect() and erase the cache entry. If no entry is found
 * for the key, the function is effectively a no-op and returns
 * @c DYAD_RC_OK.
 *
 * @note All cache operations are wrapped in a @c try / @c catch(...)
 *       block to prevent C++ exceptions from propagating into the C
 *       calling code.
 *
 * @note @p addr and @p addr_size are accepted for interface consistency
 *       but are not used — the removal is performed by
 *       @c consumer_conn_key rather than by raw address bytes.
 *
 * @param[in] ctx       DYAD context. The @c consumer_conn_key used as
 *                      the cache lookup key is read from the UCX DTL
 *                      internal state.
 * @param[in] cache     Endpoint cache handle to remove from.
 * @param[in] addr      Unused. Accepted for interface consistency.
 * @param[in] addr_size Unused. Accepted for interface consistency.
 * @param[in] worker    UCX worker passed to @c ucx_disconnect() to
 *                      progress the endpoint close operation.
 *
 * @return @c dyad_rc_t return code:
 * @retval DYAD_RC_OK      Entry removed successfully or key not found.
 * @retval DYAD_RC_SYSFAIL An unexpected C++ exception was thrown during
 *                         the cache operation.
 */
dyad_rc_t dyad_ucx_ep_cache_remove (const dyad_ctx_t *ctx,
                                    ucx_ep_cache_h cache,
                                    const ucp_address_t *addr,
                                    const size_t addr_size,
                                    ucp_worker_h worker);

/**
 * @brief Finalizes and frees the UCX endpoint cache.
 *
 * @details
 * Iterates over all entries in the cache, disconnecting and removing
 * each endpoint via @c cache_remove_impl(). After all entries are
 * removed, deletes the @c cache_type object and sets @p *cache to
 * @c nullptr.
 *
 * The iteration uses the iterator returned by @c cache_remove_impl()
 * rather than incrementing the iterator manually, since @c erase()
 * invalidates the current iterator. @c cache_remove_impl() returns
 * the iterator to the next valid entry after the erased one, making
 * this a safe and correct way to drain the entire cache.
 *
 * If @p cache is @c nullptr or @p *cache is @c nullptr, the function
 * is a no-op and returns @c DYAD_RC_OK. This allows safe calls on a
 * partially initialized or already-finalized cache.
 *
 * @note This function is called by @c dyad_dtl_ucx_finalize() as part
 *       of the full UCX DTL teardown sequence. The endpoint cache must
 *       be finalized before the UCX worker is destroyed, since
 *       @c ucx_disconnect() requires the worker to be active to
 *       progress the endpoint close operations.
 *
 * @note Unlike @c dyad_ucx_ep_cache_remove() which wraps operations
 *       in a @c try / @c catch block, this function does not — any
 *       C++ exception thrown during iteration or deletion will
 *       propagate to the caller. Since this is called only from C++
 *       translation units this is acceptable, but a @c try / @c catch
 *       wrapper could be added for robustness (see TODO).
 *
 * @todo Add a @c try / @c catch block around the iteration and
 *       @c delete to prevent C++ exceptions from propagating into
 *       the C calling code in @c dyad_dtl_ucx_finalize().
 *
 * @param[in]     ctx    DYAD context. Used for logging in
 *                       @c cache_remove_impl() → @c ucx_disconnect().
 * @param[in,out] cache  Pointer to the cache handle to finalize.
 *                       @p *cache is set to @c nullptr on return.
 *                       If @c nullptr or @p *cache is @c nullptr,
 *                       the function is a no-op.
 * @param[in]     worker UCX worker passed to @c ucx_disconnect() for
 *                       each endpoint in the cache.
 *
 * @return Always returns @c DYAD_RC_OK.
 */
dyad_rc_t dyad_ucx_ep_cache_finalize (const dyad_ctx_t *ctx,
                                      ucx_ep_cache_h *cache,
                                      ucp_worker_h worker);

#ifdef __cplusplus
}
#endif

#endif /* DYAD_DTL_UCX_EP_CACHE_H */
