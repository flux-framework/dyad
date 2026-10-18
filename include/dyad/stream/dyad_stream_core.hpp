/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_STREAM_DYAD_STREAM_CORE_HPP
#define DYAD_STREAM_DYAD_STREAM_CORE_HPP

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <climits>
#include <iostream>
#include <string>

#include <dyad/stream/dyad_params.hpp>

extern "C" {
struct dyad_ctx;
}

namespace dyad
{
/**
 * @brief Core DYAD synchronization state and operations for C++ stream interception.
 *
 * @details
 * Wraps a @c dyad_ctx and exposes the producer/consumer synchronization
 * operations needed by the C++ stream interception layer. Handles context
 * initialization, file locking, path prefix matching, and open/close
 * synchronization hooks.
 *
 * A single instance is typically embedded in a stream wrapper object. The
 * context may be initialized either from environment variables via
 * @c init(bool) or from explicit parameters via @c init(const dyad_params&).
 *
 * @note @c dyad_stream_core does not initialize the DYAD context
 *       automatically in its constructor, nor does it tear it down in its
 *       destructor. Initialization and teardown are costly operations, and
 *       C++ objects may trigger many implicit copy constructions and
 *       destructions. The caller is responsible for explicitly calling
 *       @c dyad_stream_core::init() once and managing the context lifetime.
 *       stream wrapper classes only performs shallow copy of dyad_stream_core
 *       object as deep copying is not only costly but also can be desruptive
 *       by running routines that are supposed to run only once during the run.
 */
class dyad_stream_core
{
   public:
    /** Constructs an uninitialized stream core. */
    dyad_stream_core ();

    /** Finalizes the stream core, releasing the DYAD context. */
    ~dyad_stream_core ();

    /**
     * @brief Initializes the stream core from environment variables.
     *
     * @details
     * Retrieves an existing DYAD context via @c dyad_ctx_get() or initializes
     * a new one if none exists, if @p reinit is @c true, or if the
     * @c DYAD_REINIT environment variable is set. In case of (re)initialization,
     * it relies on the environment variable-based method, @c dyad_init_env()
     * which reads configuration from environment variables. The existence of
     * environment variables @c DYAD_PATH_PRODUCER and @c DYAD_PATH_CONSUMER limits the
     * potential producer and consumer roles role of the enclosing stream wrapper
     * object. Has no effect if already initialized and neither @p reinit nor
     * the environment variable @c DYAD_REINIT is set.
     *
     * @param[in] reinit  If @c true, forces re-initialization even if already
     *                    initialized.
     */
    void init (const bool reinit = false);

    /**
     * @brief Initializes the stream core from explicit parameters.
     *
     * @details
     * Calls @c dyad_init() with the settings in @p p and retrieves the
     * resulting context. Producer and consumer roles are limited based on
     * whether the managed paths in @p p are empty. Also sets
     * @c ctx->use_fs_locks based on whether @c DYAD_HAS_STD_FSTREAM_FD
     * is set at cmake configure time.
     *
     * @param[in] p  Parameters to initialize DYAD with.
     */
    void init (const dyad_params &p);

    /**
     * @brief Logs the current DYAD configuration at INFO level.
     *
     * @param[in] msg_head  Header string printed before the configuration.
     */
    void log_info (const std::string &msg_head) const;

    /** Releases the DYAD context and resets initialization state. */
    void finalize ();

    /** Returns @c true if this instance is configured as a producer. */
    bool is_dyad_producer () const;

    /** Returns @c true if this instance is configured as a consumer. */
    bool is_dyad_consumer () const;

    /**
     * @brief Ensures a file is ready to read before a stream open.
     *
     * @details
     * Calls @c dyad_consume() if the instance is a consumer and is
     * initialized. Returns @c true if the file is ready or if no action is
     * needed (not a consumer, or not initialized).
     *
     * @param[in] path  Path to the file being opened.
     * @return @c true on success or no-op, @c false if @c dyad_consume() failed.
     */
    bool open_sync (const char *path);

    /**
     * @brief Publishes a file after a stream close.
     *
     * @details
     * Calls @c dyad_produce() if the instance is a producer and is
     * initialized. Returns @c true if the file was published or if no action
     * is needed (not a producer, or not initialized).
     *
     * @param[in] path  Path to the file being closed.
     * @return @c true on success or no-op, @c false if @c dyad_produce() failed.
     */
    bool close_sync (const char *path);

    /** Marks the stream core as initialized without calling @c init(). */
    void set_initialized ();

    /** Returns @c true if the stream core has been initialized. */
    bool chk_initialized () const;

    /** Returns @c true if @c fsync() on write is enabled in the context. */
    bool chk_fsync_write () const;

    /**
     * @brief Checks whether @p path falls under a DYAD-managed directory.
     *
     * @details
     * Delegates to @c ::cmp_canonical_path_prefix() and stores the extracted
     * relative path in @c upath, retrievable via @c get_upath().
     *
     * @param[in] is_prod  If @c true, match against the producer-managed path;
     *                     otherwise match against the consumer-managed path.
     * @param[in] path     Path to check.
     * @return @c true if @p path is under the managed directory.
     */
    bool cmp_canonical_path_prefix (bool is_prod, const char *const DYAD_RESTRICT path);

    /**
     * @brief Returns the relative path extracted by the last
     *        @c cmp_canonical_path_prefix() call.
     */
    std::string get_upath () const;

    /**
     * @brief Acquires an exclusive lock on an open file descriptor.
     * @param[in] fd  File descriptor to lock.
     * @return @c dyad_rc_t return code from @c dyad_excl_flock().
     */
    int file_lock_exclusive (int fd) const;

    /**
     * @brief Acquires a shared lock on an open file descriptor.
     * @param[in] fd  File descriptor to lock.
     * @return @c dyad_rc_t return code from @c dyad_excl_flock().
     */
    int file_lock_shared (int fd) const;

    /**
     * @brief Acquires a shared OFD lock on an open file descriptor.
     * @param[in] fd  File descriptor whose open file description to lock.
     * @return @c dyad_rc_t return code from @c dyad_ofd_shared_flock().
     */
    int file_lock_shared_ofd (int fd) const;

    /**
     * @brief Releases a lock on an open file descriptor.
     * @param[in] fd  File descriptor to unlock.
     * @return @c dyad_rc_t return code from @c dyad_release_flock().
     */
    int file_unlock (int fd) const;

   private:
    const dyad_ctx *m_ctx;    ///< Immutable pointer to the DYAD context.
    dyad_ctx *m_ctx_mutable;  ///< Mutable pointer to the DYAD context.
    bool m_initialized;       ///< Whether the stream core has been initialized.
    bool m_is_prod;           ///< Whether this instance can act as a producer.
    bool m_is_cons;           ///< Whether this instance can act as a consumer.
    char upath[PATH_MAX];     ///< Relative path from last @c cmp_canonical_path_prefix() call.
};

}  // end of namespace dyad
#endif  // DYAD_STREAM_DYAD_STREAM_CORE_HPP
