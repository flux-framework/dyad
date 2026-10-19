/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef DYAD_STREAM_DYAD_STREAM_API_HPP
#define DYAD_STREAM_DYAD_STREAM_API_HPP

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <unistd.h>  // fsync

#include <climits>  // realpath
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>

#include <dyad/stream/dyad_stream_core.hpp>

namespace dyad
{

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
/**
 * @brief Type alias that enables a template overload only when @p _Path is a
 *        filesystem path type.
 *
 * @details
 * Uses SFINAE via @c std::enable_if_t to constrain template instantiation to
 * types that satisfy the @c std::filesystem::path (or
 * @c std::experimental::filesystem::path) interface. Specifically, the
 * constraint checks that the expression
 * @c std::declval<_Path&>().make_preferred().filename() returns the same type
 * as @p _Path itself, which is a characteristic of filesystem path types.
 *
 * This alias evaluates to @p _Result (defaulting to @p _Path) when the
 * constraint is satisfied, and is a substitution failure otherwise, removing
 * the overload from the candidate set.
 *
 * Only available when compiling with C++17 or later and when
 * @c <filesystem> is available (@c __has_include(<filesystem>)).
 *
 * @tparam _Path    The type to constrain. The alias is enabled only when
 *                  @c make_preferred().filename() on @p _Path returns
 *                  @p _Path itself.
 * @tparam _Result  The type this alias resolves to when the constraint is
 *                  satisfied. Defaults to @p _Path.
 * @tparam _Path2   Deduced return type of
 *                  @c std::declval<_Path&>().make_preferred().filename().
 *                  Not intended to be specified explicitly.
 */
template <typename _Path,
          typename _Result = _Path,
          typename _Path2 = decltype (std::declval<_Path &> ().make_preferred ().filename ())>
using dyad_if_fs_path = std::enable_if_t<std::is_same_v<_Path, _Path2>, _Result>;
#endif  // c++17 filesystem

/**
 * @brief Flushes and syncs a @c std::basic_ofstream to durable storage.
 *
 * @details
 * Flushes the stream's write buffer via @c flush() and, if
 * @c DYAD_HAS_STD_FSTREAM_FD is defined, extracts the underlying file
 * descriptor by casting the stream's @c rdbuf() to an internal
 * @c my_filebuf subclass that exposes @c _M_file.fd(), then calls
 * @c fsync() on it to ensure all data is written to durable storage.
 *
 * If @c DYAD_HAS_STD_FSTREAM_FD is not defined, only @c flush() is called.
 * Data may remain in the OS page cache and is not guaranteed to be durable
 * until the file is closed or the OS flushes its buffers.
 *
 * If the stream is not open, the function returns without taking any action.
 *
 * @tparam _CharT    Character type of the stream.
 * @tparam _Traits   Character traits type of the stream.
 *
 * @param[in,out] os  The output file stream to flush and sync. Must be a
 *                    valid @c std::basic_ofstream instance.
 *
 * @note The file descriptor extraction relies on the GCC/libstdc++ internal
 *       @c _M_file.fd() member of @c std::basic_filebuf, exposed here via a
 *       local @c my_filebuf subclass. This is not portable across all standard
 *       library implementations. @c DYAD_HAS_STD_FSTREAM_FD is set
 *       automatically by CMake at configure time by attempting to compile the
 *       file descriptor extraction code; if compilation fails, the flag is not
 *       set and only @c flush() is used. See
 *       https://stackoverflow.com/questions/676787 for background.
 */
template <typename _CharT, typename _Traits>
void fsync_ofstream (std::basic_ofstream<_CharT, _Traits> &os)
{
#if defined(DYAD_HAS_STD_FSTREAM_FD)
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (os.is_open ()) {
        os.flush ();
        fsync (static_cast<my_filebuf &> (*os.rdbuf ()).handle ());
    }
#else
    if (os.is_open ()) {
        os.flush ();
    }
#endif  // DYAD_HAS_STD_FSTREAM_FD
}

/**
 * @brief Flushes and syncs a @c std::basic_fstream to durable storage.
 *
 * @details
 * Identical in behavior to @c fsync_ofstream() but operates on a
 * @c std::basic_fstream. See @c fsync_ofstream() for full details.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] os  File stream to flush and sync.
 */
template <typename _CharT, typename _Traits>
void fsync_fstream (std::basic_fstream<_CharT, _Traits> &os)
{
#if defined(DYAD_HAS_STD_FSTREAM_FD)
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (os.is_open ()) {
        os.flush ();
        fsync (static_cast<my_filebuf &> (*os.rdbuf ()).handle ());
    }
#else
    if (os.is_open ()) {
        os.flush ();
    }
#endif  // DYAD_HAS_STD_FSTREAM_FD
}

//----------------------------------------------------------------------
#if defined(DYAD_HAS_STD_FSTREAM_FD)
//----------------------------------------------------------------------
/**
 * @brief Acquires an exclusive lock on the file underlying a @c std::basic_ofstream.
 *
 * @details
 * Extracts the file descriptor via a local @c my_filebuf subclass and calls
 * @c core.file_lock_exclusive() on it. Has no effect if the stream is not open.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] os    Output file stream to lock.
 * @param[in]     core  DYAD stream core providing the locking operation.
 *
 * @note Relies on the GCC/libstdc++ internal @c _M_file.fd(). Not portable
 *       across all standard library implementations.
 */
template <typename _CharT, typename _Traits>
void lock_exclusive_ofstream (std::basic_ofstream<_CharT, _Traits> &os,
                              const dyad_stream_core &core)
{
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (os.is_open ()) {
        int fd = static_cast<my_filebuf &> (*os.rdbuf ()).handle ();
        core.file_lock_exclusive (fd);
    }
}

/**
 * @brief Acquires an exclusive lock on the file underlying a @c std::basic_fstream.
 *
 * @details
 * Identical in behavior to @c lock_exclusive_ofstream() but operates on a
 * @c std::basic_fstream. See @c lock_exclusive_ofstream() for full details.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] os    File stream to lock.
 * @param[in]     core  DYAD stream core providing the locking operation.
 */
template <typename _CharT, typename _Traits>
void lock_exclusive_fstream (std::basic_fstream<_CharT, _Traits> &os, const dyad_stream_core &core)
{
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (os.is_open ()) {
        int fd = static_cast<my_filebuf &> (*os.rdbuf ()).handle ();
        core.file_lock_exclusive (fd);
    }
}

/**
 * @brief Acquires a shared lock on the file underlying a @c std::basic_ifstream.
 *
 * @details
 * Extracts the file descriptor via a local @c my_filebuf subclass and calls
 * @c core.file_lock_shared_ofd() on it, allowing multiple concurrent readers.
 * Has no effect if the stream is not open.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] is    Input file stream to lock.
 * @param[in]     core  DYAD stream core providing the locking operation.
 *
 * @note Relies on the GCC/libstdc++ internal @c _M_file.fd(). Not portable
 *       across all standard library implementations.
 */
template <typename _CharT, typename _Traits>
void lock_shared_ifstream (std::basic_ifstream<_CharT, _Traits> &is, const dyad_stream_core &core)
{
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (is.is_open ()) {
        int fd = static_cast<my_filebuf &> (*is.rdbuf ()).handle ();
        core.file_lock_shared_ofd (fd);
    }
}
/**
 * @brief Acquires a shared lock on the file underlying a @c std::basic_fstream.
 *
 * @details
 * Identical in behavior to @c lock_shared_ifstream() but operates on a
 * @c std::basic_fstream. See @c lock_shared_ifstream() for full details.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] is    File stream to lock.
 * @param[in]     core  DYAD stream core providing the locking operation.
 */
template <typename _CharT, typename _Traits>
void lock_shared_fstream (std::basic_fstream<_CharT, _Traits> &is, const dyad_stream_core &core)
{
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (is.is_open ()) {
        int fd = static_cast<my_filebuf &> (*is.rdbuf ()).handle ();
        core.file_lock_shared (fd);
    }
}

/**
 * @brief Releases a lock on the file underlying a @c std::basic_ofstream.
 *
 * @details
 * Extracts the file descriptor via a local @c my_filebuf subclass and calls
 * @c core.file_unlock() on it to release any lock previously acquired via
 * @c lock_exclusive_ofstream(). Has no effect if the stream is not open.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] os    Output file stream to unlock.
 * @param[in]     core  DYAD stream core providing the unlock operation.
 *
 * @note Relies on the GCC/libstdc++ internal @c _M_file.fd(). Not portable
 *       across all standard library implementations.
 */
template <typename _CharT, typename _Traits>
void unlock_ofstream (std::basic_ofstream<_CharT, _Traits> &os, const dyad_stream_core &core)
{
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (os.is_open ()) {
        int fd = static_cast<my_filebuf &> (*os.rdbuf ()).handle ();
        core.file_unlock (fd);
    }
}

/**
 * @brief Releases a lock on the file underlying a @c std::basic_ifstream.
 *
 * @details
 * Identical in behavior to @c unlock_ofstream() but operates on a
 * @c std::basic_ifstream. See @c unlock_ofstream() for full details.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] is    Input file stream to unlock.
 * @param[in]     core  DYAD stream core providing the unlock operation.
 */
template <typename _CharT, typename _Traits>
void unlock_ifstream (std::basic_ifstream<_CharT, _Traits> &is, const dyad_stream_core &core)
{
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (is.is_open ()) {
        int fd = static_cast<my_filebuf &> (*is.rdbuf ()).handle ();
        core.file_unlock (fd);
    }
}

/**
 * @brief Releases a lock on the file underlying a @c std::basic_fstream.
 *
 * @details
 * Identical in behavior to @c unlock_ofstream() but operates on a
 * @c std::basic_fstream. See @c unlock_ofstream() for full details.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type of the stream.
 * @param[in,out] os    File stream to unlock.
 * @param[in]     core  DYAD stream core providing the unlock operation.
 */
template <typename _CharT, typename _Traits>
void unlock_fstream (std::basic_fstream<_CharT, _Traits> &os, const dyad_stream_core &core)
{
    class my_filebuf : public std::basic_filebuf<_CharT>
    {
       public:
        int handle ()
        {
            return this->_M_file.fd ();
        }
    };

    if (os.is_open ()) {
        int fd = static_cast<my_filebuf &> (*os.rdbuf ()).handle ();
        core.file_unlock (fd);
    }
}
//----------------------------------------------------------------------
#endif  // DYAD_HAS_STD_FSTREAM_FD
//----------------------------------------------------------------------

/**
 * @defgroup dyad_cpp_lock_macros DYAD C++ stream locking macros
 * @{
 *
 * @brief Macros for acquiring and releasing locks on C++ stream objects.
 *
 * @details
 * When @c DYAD_HAS_STD_FSTREAM_FD is defined (determined automatically by
 * CMake at configure time by testing whether the file descriptor can be
 * extracted from a @c std::basic_filebuf via the GCC/libstdc++ internal
 * @c _M_file.fd()), these macros expand to the corresponding lock functions:
 *  - @c DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM  → @c lock_exclusive_ofstream()
 *  - @c DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM   → @c lock_exclusive_fstream()
 *  - @c DYAD_SHARED_LOCK_CPP_IFSTREAM     → @c lock_shared_ifstream()
 *  - @c DYAD_SHARED_LOCK_CPP_FSTREAM      → @c lock_shared_fstream()
 *  - @c DYAD_UNLOCK_CPP_OFSTREAM          → @c unlock_ofstream()
 *  - @c DYAD_UNLOCK_CPP_IFSTREAM          → @c unlock_ifstream()
 *  - @c DYAD_UNLOCK_CPP_FSTREAM           → @c unlock_fstream()
 *
 * When @c DYAD_HAS_STD_FSTREAM_FD is not defined, all macros expand to
 * nothing. Filesystem locking is silently disabled and synchronization
 * falls back to KVS-based coordination only, which may perform worse for
 * co-located producers and consumers on node-local storage since the
 * producer cannot lock the file it is writing.
 *
 * @see dyad_stream_core::init(const dyad_params&) where @c use_fs_locks
 *      is set accordingly.
 */
#if defined(DYAD_HAS_STD_FSTREAM_FD)
#define DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM(_os_, _core_) lock_exclusive_ofstream (_os_, _core_)
#define DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM(_os_, _core_) lock_exclusive_fstream (_os_, _core_)
#define DYAD_SHARED_LOCK_CPP_IFSTREAM(_os_, _core_) lock_shared_ifstream (_os_, _core_)
#define DYAD_SHARED_LOCK_CPP_FSTREAM(_os_, _core_) lock_shared_fstream (_os_, _core_)
#define DYAD_UNLOCK_CPP_OFSTREAM(_os_, _core_) unlock_ofstream (_os_, _core_)
#define DYAD_UNLOCK_CPP_IFSTREAM(_os_, _core_) unlock_ifstream (_os_, _core_)
#define DYAD_UNLOCK_CPP_FSTREAM(_os_, _core_) unlock_fstream (_os_, _core_)
#else  // DYAD_HAS_STD_FSTREAM_FD
#define DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM(_os_, _core_)
#define DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM(_os_, _core_)
#define DYAD_SHARED_LOCK_CPP_IFSTREAM(_os_, _core_)
#define DYAD_SHARED_LOCK_CPP_FSTREAM(_os_, _core_)
#define DYAD_UNLOCK_CPP_OFSTREAM(_os_, _core_)
#define DYAD_UNLOCK_CPP_IFSTREAM(_os_, _core_)
#define DYAD_UNLOCK_CPP_FSTREAM(_os_, _core_)
#endif  // DYAD_HAS_STD_FSTREAM_FD
/** @} */

//=============================================================================
// basic_ifstream_dyad (std::basic_ifstream wrapper)
//=============================================================================

/**
 * @brief A drop-in replacement for @c std::basic_ifstream that integrates
 *        DYAD consumer-side synchronization.
 *
 * @details
 * Wraps @c std::basic_ifstream and intercepts @c open() and @c close() to
 * call @c dyad_consume() via the embedded @c dyad_stream_core, ensuring the
 * file is ready to read before the stream is opened. All other stream
 * operations are delegated to the underlying @c std::basic_ifstream.
 *
 * The interface mirrors @c std::basic_ifstream as closely as possible,
 * including C++11 move semantics, @c std::string overloads, and (in C++17)
 * @c std::filesystem::path overloads, conditionally compiled based on the
 * standard version in use.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type. Defaults to
 *                  @c std::char_traits<_CharT>.
 */
template <typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_ifstream_dyad
{
   public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_ifstream = typename std::basic_ifstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    /**
     * @brief Constructs a stream with an existing @c dyad_stream_core.
     *
     * @details
     * Copies @p core into the embedded @c m_core and allocates the underlying
     * stream without opening it. Does not call @c m_core.init() since the
     * provided core is assumed to be already initialized.
     *
     * @param[in] core  Initialized stream core to copy.
     */
    basic_ifstream_dyad (const dyad_stream_core &core);

    /** Constructs an unopened stream and initializes the core from environment
     *  variables via @c m_core.init(). */
    basic_ifstream_dyad ();

    /**
     * @brief Constructs and opens the stream, triggering consumer synchronization.
     *
     * @details
     * Initializes the core from environment variables, calls @c open_sync() to
     * ensure the file is ready via @c dyad_consume(), then opens the underlying
     * stream. After a successful open, acquires a shared OFD lock when
     * filesystem locking is available. @c m_filename is set only if the stream
     * opened successfully.
     *
     * @param[in] filename  Path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::in.
     */
    explicit basic_ifstream_dyad (const char *filename, ios_base::openmode mode = ios_base::in);

    /**
     * @brief Destroys the stream, releasing the underlying @c basic_ifstream.
     *
     * @details
     * In C++03, explicitly deletes the raw pointer. In C++11 and later, resets
     * the @c unique_ptr. Has no effect if the stream pointer is already
     * @c nullptr.
     */
    ~basic_ifstream_dyad ();

    /**
     * @brief Opens the file, triggering consumer synchronization.
     *
     * @details
     * Calls @c open_sync() to ensure the file is ready via @c dyad_consume(),
     * then opens the underlying stream. After a successful open, acquires a
     * shared OFD lock via @c DYAD_SHARED_LOCK_CPP_IFSTREAM when filesystem
     * locking is available. If the stream pointer is @c nullptr, returns
     * without action. @c m_filename is set only if the stream opened
     * successfully.
     *
     * @param[in] filename  Path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::in.
     */
    void open (const char *filename, ios_base::openmode mode = ios_base::in);

#if __cplusplus < 201103L
    /** Returns @c true if the underlying stream is open. Returns @c false if
     *  the stream pointer is @c nullptr. */
    bool is_open ();
#else
    /** Constructs and opens the stream from a @c std::string filename. See the
     *  @c const char* constructor for full details. */
    explicit basic_ifstream_dyad (const string &filename, ios_base::openmode mode = ios_base::in);

    /** Copy construction is disabled. */
    basic_ifstream_dyad (const basic_ifstream_dyad &) = delete;

    /** Move constructor. Transfers ownership of the stream and core from @p rhs. */
    basic_ifstream_dyad (basic_ifstream_dyad &&rhs);
#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    /**
     * @brief Constructs and opens the stream from a @c std::filesystem::path.
     *
     * @details
     * Converts @p filepath to a C string via @c filepath.c_str() and delegates
     * to the same initialization sequence as the @c const char* constructor.
     * Only available in C++17 and later when @c <filesystem> is present.
     *
     * @param[in] filepath  Filesystem path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::in.
     */
    template <typename _Path, typename _Require = dyad_if_fs_path<_Path> >
    basic_ifstream_dyad (const _Path &filepath, std::ios_base::openmode mode = std::ios_base::in);
#endif  // c++17 filesystem

    /** Opens the file from a @c std::string filename. Delegates to the
     *  @c const char* overload. */
    void open (const string &filename, ios_base::openmode mode = ios_base::in);

    /** Returns @c true if the underlying stream is open. Returns @c false if
     *  the stream pointer is @c nullptr. */
    bool is_open () const;

    /** Move assignment. Takes ownership of @p rhs's stream and core. */
    basic_ifstream_dyad &operator= (basic_ifstream_dyad &&rhs);

    /** Swaps the underlying stream with @p rhs. Has no effect if the stream
     *  pointer is @c nullptr (TODO: set fail bit).  */
    void swap (basic_ifstream_dyad &rhs);
#endif

    /**
     * @brief Closes the stream and releases its shared OFD lock.
     *
     * @details
     * Closing the underlying descriptor releases its OFD lock.
     * Has no effect if the stream pointer is @c nullptr.
     */
    void close ();

    /** Returns the underlying stream buffer, or @c nullptr if the stream
     *  pointer is @c nullptr. */
    filebuf *rdbuf () const;

    /** Returns a reference to the underlying @c std::basic_ifstream.
     *  @warning If the stream pointer is @c nullptr, behavior is undefined
     *  (TODO: throw).  */
    basic_ifstream &get_stream ();

    /**
     * @brief Reinitializes the stream core from an existing @c dyad_stream_core.
     *
     * @details
     * Copies @p core into @c m_core, marks it as initialized via
     * @c set_initialized(), and logs the new state via @c log_info().
     *
     * @param[in] core  Stream core to copy into this instance.
     */
    void init (const dyad_stream_core &core);

    /** Allow read-only access to the embedded @c dyad_stream_core. */
    const dyad_stream_core &core () const
    {
        return m_core;
    }

   private:
    /// Embedded DYAD stream core managing synchronization state.
    dyad_stream_core m_core;

#if __cplusplus < 201103L
    basic_ifstream m_stream;  ///< Underlying stream (C++03)
#else
    std::unique_ptr<basic_ifstream> m_stream;  ///< Underlying stream (C++11 or newer)
#endif

    std::string m_filename;  ///< Most recently opened filename.
};

/** @c basic_ifstream_dyad specialization for @c char. */
using ifstream_dyad = basic_ifstream_dyad<char>;

/** @c basic_ifstream_dyad specialization for @c wchar_t. */
using wifstream_dyad = basic_ifstream_dyad<wchar_t>;

#if __cplusplus < 201103L  //----------------------------------------------------
template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const dyad_stream_core &core)
    : m_core (core)
{
    m_stream = new basic_ifstream ();
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad ()
{
    m_core.init ();
    m_stream = new basic_ifstream ();
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const char *filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    int guard_fd = -1;
    m_core.open_sync (filename, &guard_fd);
    m_stream = new basic_ifstream (filename, mode);
    if ((m_stream != nullptr) && (*m_stream)) {
        DYAD_SHARED_LOCK_CPP_IFSTREAM (*m_stream, m_core);
        m_filename = std::string{filename};
    }
    m_core.close_consume_guard (guard_fd);
}

template <typename _CharT, typename _Traits>
bool basic_ifstream_dyad<_CharT, _Traits>::is_open ()
{
    return ((m_stream != nullptr) && (m_stream->is_open ()));
}
#else  //-----------------------------------------------------------------------
template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const dyad_stream_core &core)
    : m_core (core)
{
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream ());
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad ()
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream ());
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const char *filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    int guard_fd = -1;
    m_core.open_sync (filename, &guard_fd);
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        DYAD_SHARED_LOCK_CPP_IFSTREAM (*m_stream, m_core);
        m_filename = std::string{filename};
    }
    m_core.close_consume_guard (guard_fd);
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const string &filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    int guard_fd = -1;
    m_core.open_sync (filename.c_str (), &guard_fd);
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        DYAD_SHARED_LOCK_CPP_IFSTREAM (*m_stream, m_core);
        m_filename = filename;
    }
    m_core.close_consume_guard (guard_fd);
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (basic_ifstream_dyad &&rhs)
    : m_stream (std::move (rhs.m_stream)), m_core (std::move (rhs.m_core))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template <typename _CharT, typename _Traits>
template <typename _Path, typename _Require>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const _Path &filepath,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    int guard_fd = -1;
    m_core.open_sync (filepath.c_str (), &guard_fd);
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream (filepath.c_str (), mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        DYAD_SHARED_LOCK_CPP_IFSTREAM (*m_stream, m_core);
        m_filename = std::string{filepath.c_str ()};
    }
    m_core.close_consume_guard (guard_fd);
}
#endif  // c++17 filesystem

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits> &basic_ifstream_dyad<_CharT, _Traits>::operator= (
    basic_ifstream_dyad &&rhs)
{
    m_stream = std::move (rhs.m_stream);
    m_core = std::move (rhs.m_core);
    return (*this);
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::~basic_ifstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
#if __cplusplus < 201103L
    delete m_stream;
    m_stream = nullptr;
#else
    m_stream.reset ();
#endif
}

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::open (const string &filename,
                                                 std::ios_base::openmode mode)
{
    open (filename.c_str (), mode);
}

template <typename _CharT, typename _Traits>
bool basic_ifstream_dyad<_CharT, _Traits>::is_open () const
{
    return ((m_stream != nullptr) && (m_stream->is_open ()));
}

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::swap (basic_ifstream_dyad &rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (*rhs.m_stream);
}
#endif  //-----------------------------------------------------------------------

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::open (const char *filename, std::ios_base::openmode mode)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    int guard_fd = -1;
    m_core.open_sync (filename, &guard_fd);
    m_stream->open (filename, mode);
    if ((m_stream != nullptr) && (*m_stream)) {
        DYAD_SHARED_LOCK_CPP_IFSTREAM (*m_stream, m_core);
        m_filename = std::string{filename};
    }
    m_core.close_consume_guard (guard_fd);
}

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::close ()
{
    if (m_stream == nullptr) {
        return;
    }
    m_stream->close ();
}

template <typename _CharT, typename _Traits>
std::filebuf *basic_ifstream_dyad<_CharT, _Traits>::rdbuf () const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template <typename _CharT, typename _Traits>
std::basic_ifstream<_CharT, _Traits> &basic_ifstream_dyad<_CharT, _Traits>::get_stream ()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::init (const dyad_stream_core &core)
{
    m_core = core;
    m_core.set_initialized ();
    m_core.log_info ("Stream core state is set");
}

//=============================================================================
// basic_ofstream_dyad (std::basic_ofstream wrapper)
//=============================================================================

/**
 * @brief A drop-in replacement for @c std::basic_ofstream that integrates
 *        DYAD producer-side synchronization.
 *
 * @details
 * Wraps @c std::basic_ofstream and intercepts @c open() and @c close() to
 * acquire an exclusive file lock and call @c dyad_produce() via the embedded
 * @c dyad_stream_core, notifying consumers that the file is ready. If
 * @c DYAD_HAS_STD_FSTREAM_FD is set, locking is only applied when the file
 * falls under the producer-managed path. Optionally calls @c fsync() before
 * closing if @c ctx->fsync_write is enabled.
 *
 * The interface mirrors @c std::basic_ofstream as closely as possible,
 * including C++11 move semantics, @c std::string overloads, and (in C++17)
 * @c std::filesystem::path overloads, conditionally compiled based on the
 * standard version in use.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type. Defaults to
 *                  @c std::char_traits<_CharT>.
 */
template <typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_ofstream_dyad
{
   public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_ofstream = typename std::basic_ofstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    /**
     * @brief Constructs a stream with an existing @c dyad_stream_core.
     *
     * @details
     * Copies @p core into the embedded @c m_core and allocates the underlying
     * stream without opening it. Does not call @c m_core.init() since the
     * provided core is assumed to be already initialized.
     *
     * @param[in] core  Initialized stream core to copy.
     */
    basic_ofstream_dyad (const dyad_stream_core &core);

    /** Constructs an unopened stream and initializes the core from environment
     *  variables via @c m_core.init(). */
    basic_ofstream_dyad ();

    /**
     * @brief Constructs and opens the stream, acquiring an exclusive lock if
     *        the file is under the producer-managed path.
     *
     * @details
     * Initializes the core from environment variables and opens the underlying
     * stream. If @c DYAD_HAS_STD_FSTREAM_FD is set and the file falls under
     * the producer-managed path, acquires an exclusive lock via
     * @c DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM and sets @c m_filename.
     *
     * @param[in] filename  Path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::out.
     */
    explicit basic_ofstream_dyad (const char *filename, ios_base::openmode mode = ios_base::out);

    /**
     * @brief Destroys the stream, syncing and publishing if still open.
     *
     * @details
     * If the stream is still open, optionally calls @c fsync_ofstream() if
     * @c ctx->fsync_write is enabled, releases the exclusive lock, resets the
     * stream, then calls @c close_sync() to publish the file via
     * @c dyad_produce(). If the stream is already closed, simply resets it.
     */
    ~basic_ofstream_dyad ();

    /**
     * @brief Opens the file and acquires an exclusive lock if under the
     *        producer-managed path.
     *
     * @details
     * Opens the underlying stream, then if @c DYAD_HAS_STD_FSTREAM_FD is set
     * and the file is under the producer-managed path, acquires an exclusive
     * lock via @c DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM and sets @c m_filename.
     * Has no effect if the stream pointer is @c nullptr.
     *
     * @param[in] filename  Path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::out.
     */
    void open (const char *filename, ios_base::openmode mode = ios_base::out);

#if __cplusplus < 201103L
    /** Returns @c true if the underlying stream is open. Returns @c false if
     *  the stream pointer is @c nullptr. */
    bool is_open ();
#else
    /** Constructs and opens the stream from a @c std::string filename. See the
     *  @c const char* constructor for full details. */
    explicit basic_ofstream_dyad (const string &filename, ios_base::openmode mode = ios_base::out);

    /** Copy construction is disabled. */
    basic_ofstream_dyad (const basic_ofstream_dyad &) = delete;

    /** Move constructor. Transfers ownership of the stream and core from @p rhs. */
    basic_ofstream_dyad (basic_ofstream_dyad &&rhs);
#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    /**
     * @brief Constructs and opens the stream from a @c std::filesystem::path.
     *
     * @details
     * Converts @p filepath to a C string and delegates to the same
     * initialization sequence as the @c const char* constructor.
     * Only available in C++17 and later when @c <filesystem> is present.
     *
     * @param[in] filepath  Filesystem path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::out.
     */
    template <typename _Path, typename _Require = dyad_if_fs_path<_Path> >
    basic_ofstream_dyad (const _Path &filepath, std::ios_base::openmode mode = std::ios_base::out);
#endif  // c++17 filesystem

    /** Opens the file from a @c std::string filename. Delegates to the
     *  @c const char* overload. */
    void open (const string &filename, ios_base::openmode mode = ios_base::out);

    /** Returns @c true if the underlying stream is open. Returns @c false if
     *  the stream pointer is @c nullptr. */
    bool is_open () const;

    /** Move assignment. Transfers ownership of the stream and core from @p rhs. */
    basic_ofstream_dyad &operator= (basic_ofstream_dyad &&rhs);

    /** Swaps the underlying stream with @p rhs. Has no effect if the stream
     *  pointer is @c nullptr. */
    void swap (basic_ofstream_dyad &rhs);
#endif

    /**
     * @brief Closes the stream, syncing and publishing the file.
     *
     * @details
     * Optionally calls @c fsync_ofstream() if @c ctx->fsync_write is enabled,
     * releases the exclusive lock via @c DYAD_UNLOCK_CPP_OFSTREAM, closes the
     * underlying stream, then calls @c close_sync() to publish the file via
     * @c dyad_produce(). Has no effect if the stream pointer is @c nullptr.
     */
    void close ();

    /** Returns the underlying stream buffer, or @c nullptr if the stream
     *  pointer is @c nullptr. */
    filebuf *rdbuf () const;

    /** Returns a reference to the underlying @c std::basic_ofstream. */
    basic_ofstream &get_stream ();

    /**
     * @brief Reinitializes the stream core from an existing @c dyad_stream_core.
     *
     * @details
     * Copies @p core into @c m_core, marks it as initialized via
     * @c set_initialized(), and logs the new state via @c log_info().
     *
     * @param[in] core  Stream core to copy into this instance.
     */
    void init (const dyad_stream_core &core);

    /** Allow read-only access to the embedded @c dyad_stream_core. */
    const dyad_stream_core &core () const
    {
        return m_core;
    }

   private:
    /// Embedded DYAD stream core managing synchronization state.
    dyad_stream_core m_core;

#if __cplusplus < 201103L
    basic_ofstream m_stream;  ///< Underlying stream (C++03).
#else
    std::unique_ptr<basic_ofstream> m_stream;  ///< Underlying stream (C++11 and later).
#endif

    std::string m_filename;  ///< Most recently opened filename, set when lock is acquired.
};

/** @c basic_ofstream_dyad specialization for @c char. */
using ofstream_dyad = basic_ofstream_dyad<char>;

/** @c basic_ofstream_dyad specialization for @c wchar_t. */
using wofstream_dyad = basic_ofstream_dyad<wchar_t>;

#if __cplusplus < 201103L  //----------------------------------------------------
template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const dyad_stream_core &core)
    : m_core (core)
{
    m_stream = new basic_ofstream ();
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad ()
{
    m_core.init ();
    m_stream = new basic_ofstream ();
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const char *filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_stream = new basic_ofstream (filename, mode);

    if ((m_stream != nullptr) && (*m_stream)
#if DYAD_HAS_STD_FSTREAM_FD
        && m_core.cmp_canonical_path_prefix (true, filename)
#endif  // DYAD_HAS_STD_FSTREAM_FD
    ) {
        DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
bool basic_ofstream_dyad<_CharT, _Traits>::is_open ()
{
    return ((m_stream != nullptr) && (m_stream->is_open ()));
}
#else  //-----------------------------------------------------------------------
template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const dyad_stream_core &core)
    : m_core (core)
{
    m_stream = std::unique_ptr<basic_ofstream> (new basic_ofstream ());
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad ()
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_ofstream> (new basic_ofstream ());
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const char *filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_ofstream> (new basic_ofstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)
#if DYAD_HAS_STD_FSTREAM_FD
        && m_core.cmp_canonical_path_prefix (true, filename)
#endif  // DYAD_HAS_STD_FSTREAM_FD
    ) {
        DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const string &filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_ofstream> (new basic_ofstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)
#if DYAD_HAS_STD_FSTREAM_FD
        && m_core.cmp_canonical_path_prefix (true, (const char *const)filename.c_str ())
#endif  // DYAD_HAS_STD_FSTREAM_FD
    ) {
        DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        m_filename = filename;
    }
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (basic_ofstream_dyad &&rhs)
    : m_stream (std::move (rhs.m_stream)),
      m_core (std::move (rhs.m_core)),
      m_filename (std::move (rhs.m_filename))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template <typename _CharT, typename _Traits>
template <typename _Path, typename _Require>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const _Path &filepath,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_ofstream> (
        new basic_ofstream ((const char *const)filepath.c_str (), mode));

    if ((m_stream != nullptr) && (*m_stream)
#if DYAD_HAS_STD_FSTREAM_FD
        && m_core.cmp_canonical_path_prefix (true, filepath.c_str ())
#endif  // DYAD_HAS_STD_FSTREAM_FD
    ) {
        DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        m_filename = std::string{filepath.c_str ()};
    }
}
#endif  // c++17 filesystem

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits> &basic_ofstream_dyad<_CharT, _Traits>::operator= (
    basic_ofstream_dyad &&rhs)
{
    m_stream = std::move (rhs.m_stream);
    m_core = std::move (rhs.m_core);
    m_filename = std::move (rhs.m_filename);
    return (*this);
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::~basic_ofstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    if (m_stream->is_open ()) {
        if (m_core.chk_fsync_write ()) {
            fsync_ofstream (*m_stream);
        }
        DYAD_UNLOCK_CPP_OFSTREAM (*m_stream, m_core);
#if __cplusplus < 201103L
        delete m_stream;
        m_stream = nullptr;
#else
        m_stream.reset ();
#endif
        m_core.close_sync (m_filename.c_str ());
    } else {
#if __cplusplus < 201103L
        delete m_stream;
        m_stream = nullptr;
#else
        m_stream.reset ();
#endif
    }
}

template <typename _CharT, typename _Traits>
void basic_ofstream_dyad<_CharT, _Traits>::open (const string &filename,
                                                 std::ios_base::openmode mode)
{
    open (filename.c_str (), mode);
}

template <typename _CharT, typename _Traits>
bool basic_ofstream_dyad<_CharT, _Traits>::is_open () const
{
    return ((m_stream != nullptr) && (m_stream->is_open ()));
}

template <typename _CharT, typename _Traits>
void basic_ofstream_dyad<_CharT, _Traits>::swap (basic_ofstream_dyad &rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (*rhs.m_stream);
}
#endif  //-----------------------------------------------------------------------

template <typename _CharT, typename _Traits>
void basic_ofstream_dyad<_CharT, _Traits>::open (const char *filename, std::ios_base::openmode mode)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->open (filename, mode);
    if ((*m_stream)
#if DYAD_HAS_STD_FSTREAM_FD
        && m_core.cmp_canonical_path_prefix (true, filename)
#endif  // DYAD_HAS_STD_FSTREAM_FD
    ) {
        DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
void basic_ofstream_dyad<_CharT, _Traits>::close ()
{
    if (m_stream == nullptr) {
        return;
    }
    if (m_core.chk_fsync_write ()) {
        fsync_ofstream (*m_stream);
    }
    DYAD_UNLOCK_CPP_OFSTREAM (*m_stream, m_core);
    m_stream->close ();
    m_core.close_sync (m_filename.c_str ());
}

template <typename _CharT, typename _Traits>
std::filebuf *basic_ofstream_dyad<_CharT, _Traits>::rdbuf () const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template <typename _CharT, typename _Traits>
std::basic_ofstream<_CharT, _Traits> &basic_ofstream_dyad<_CharT, _Traits>::get_stream ()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

template <typename _CharT, typename _Traits>
void basic_ofstream_dyad<_CharT, _Traits>::init (const dyad_stream_core &core)
{
    m_core = core;
    m_core.set_initialized ();
    m_core.log_info ("Stream core state is set");
}

//=============================================================================
// basic_fstream_dyad (std::basic_fstream wrapper)
//=============================================================================

/**
 * @brief A drop-in replacement for @c std::basic_fstream that integrates
 *        DYAD producer and consumer-side synchronization.
 *
 * @details
 * Wraps @c std::basic_fstream and intercepts @c open() and @c close() to
 * perform both consumer and producer synchronization via the embedded
 * @c dyad_stream_core. On open, calls @c open_sync() to ensure the file is
 * ready via @c dyad_consume(). If the instance is a producer, also acquires
 * an exclusive lock (conditioned on @c DYAD_HAS_STD_FSTREAM_FD and path
 * prefix matching). On close, if the instance is a producer, optionally
 * calls @c fsync_fstream() and releases the lock before calling
 * @c close_sync() to publish the file via @c dyad_produce().
 *
 * The interface mirrors @c std::basic_fstream as closely as possible,
 * including C++11 move semantics, @c std::string overloads, and (in C++17)
 * @c std::filesystem::path overloads, conditionally compiled based on the
 * standard version in use.
 *
 * @tparam _CharT   Character type of the stream.
 * @tparam _Traits  Character traits type. Defaults to
 *                  @c std::char_traits<_CharT>.
 */
template <typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_fstream_dyad
{
   public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_fstream = typename std::basic_fstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    /**
     * @brief Constructs a stream with an existing @c dyad_stream_core.
     *
     * @details
     * Copies @p core into the embedded @c m_core and allocates the underlying
     * stream without opening it. Does not call @c m_core.init() since the
     * provided core is assumed to be already initialized.
     *
     * @param[in] core  Initialized stream core to copy.
     */
    basic_fstream_dyad (const dyad_stream_core &core);

    /** Constructs an unopened stream and initializes the core from environment
     *  variables via @c m_core.init(). */
    basic_fstream_dyad ();

    /**
     * @brief Constructs and opens the stream with both consumer and producer
     *        synchronization.
     *
     * @details
     * Initializes the core from environment variables, calls @c open_sync()
     * to ensure the file is ready via @c dyad_consume(), then opens the
     * underlying stream. If the instance is a producer and the stream opened
     * successfully, acquires an exclusive lock (conditioned on
     * @c DYAD_HAS_STD_FSTREAM_FD and path prefix matching in C++03) and sets
     * @c m_filename.
     *
     * @param[in] filename  Path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::in|out.
     */
    explicit basic_fstream_dyad (const char *filename,
                                 ios_base::openmode mode = ios_base::in | ios_base::out);

    /**
     * @brief Destroys the stream, syncing and publishing if still open.
     *
     * @details
     * If the stream is still open and the instance is a producer, optionally
     * calls @c fsync_fstream() if @c ctx->fsync_write is enabled, releases
     * the exclusive lock, resets the stream, then calls @c close_sync() to
     * publish the file via @c dyad_produce(). If the stream is already closed,
     * simply resets it.
     */
    ~basic_fstream_dyad ();

    /**
     * @brief Opens the file with both consumer and producer synchronization.
     *
     * @details
     * Calls @c open_sync() to ensure the file is ready via @c dyad_consume(),
     * then opens the underlying stream. If the instance is a producer and the
     * stream opened successfully, acquires an exclusive lock (conditioned on
     * @c DYAD_HAS_STD_FSTREAM_FD and path prefix matching) and sets
     * @c m_filename. Has no effect if the stream pointer is @c nullptr.
     *
     * @param[in] filename  Path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::in|out.
     */
    void open (const char *filename, ios_base::openmode mode = ios_base::in | ios_base::out);

#if __cplusplus < 201103L
    /** Returns @c true if the underlying stream is open. Returns @c false if
     *  the stream pointer is @c nullptr. */
    bool is_open ();
#else
    /** Constructs and opens the stream from a @c std::string filename. See the
     *  @c const char* constructor for full details. */
    explicit basic_fstream_dyad (const string &filename,
                                 ios_base::openmode mode = ios_base::in | ios_base::out);
    /** Copy construction is disabled. */
    basic_fstream_dyad (const basic_fstream_dyad &) = delete;

    /** Move constructor. Transfers ownership of the stream and core from @p rhs. */
    basic_fstream_dyad (basic_fstream_dyad &&rhs);

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    /**
     * @brief Constructs and opens the stream from a @c std::filesystem::path.
     *
     * @details
     * Converts @p filepath to a C string and delegates to the same
     * initialization sequence as the @c const char* constructor.
     * Only available in C++17 and later when @c <filesystem> is present.
     *
     * @param[in] filepath  Filesystem path to the file to open.
     * @param[in] mode      Open mode. Defaults to @c ios_base::out.
     */
    template <typename _Path, typename _Require = dyad_if_fs_path<_Path> >
    basic_fstream_dyad (const _Path &filepath, std::ios_base::openmode mode = std::ios_base::out);
#endif  // c++17 filesystem

    /** Opens the file from a @c std::string filename. Delegates to the
     *  @c const char* overload. */
    void open (const string &filename, ios_base::openmode mode = ios_base::in | ios_base::out);

    /** Returns @c true if the underlying stream is open. Returns @c false if
     *  the stream pointer is @c nullptr. */
    bool is_open () const;

    /** Move assignment. Transfers ownership of the stream and core from @p rhs. */
    basic_fstream_dyad &operator= (basic_fstream_dyad &&rhs);

    /** Swaps the underlying stream with @p rhs. Has no effect if the stream
     *  pointer is @c nullptr. */
    void swap (basic_fstream_dyad &rhs);
#endif

    /**
     * @brief Closes the stream, syncing and publishing the file if a producer.
     *
     * @details
     * If the instance is a producer, optionally calls @c fsync_fstream() if
     * @c ctx->fsync_write is enabled, releases the exclusive lock via
     * @c DYAD_UNLOCK_CPP_FSTREAM, closes the underlying stream, then calls
     * @c close_sync() to publish the file via @c dyad_produce(). Has no
     * effect if the stream pointer is @c nullptr.
     */
    void close ();

    /** Returns the underlying stream buffer, or @c nullptr if the stream
     *  pointer is @c nullptr. */
    filebuf *rdbuf () const;

    /** Returns a reference to the underlying @c std::basic_fstream. */
    basic_fstream &get_stream ();

    /**
     * @brief Reinitializes the stream core from an existing @c dyad_stream_core.
     *
     * @details
     * Copies @p core into @c m_core, marks it as initialized via
     * @c set_initialized(), and logs the new state via @c log_info().
     *
     * @param[in] core  Stream core to copy into this instance.
     */
    void init (const dyad_stream_core &core);

    /** Allow read-only access to the embedded @c dyad_stream_core. */
    const dyad_stream_core &core () const
    {
        return m_core;
    }

   private:
    /// Embedded DYAD stream core managing synchronization state.
    dyad_stream_core m_core;

#if __cplusplus < 201103L
    basic_fstream m_stream;  ///< Underlying stream (C++03).
#else
    std::unique_ptr<basic_fstream> m_stream;  ///< Underlying stream (C++11 and later).
#endif

    std::string m_filename;  ///< Most recently opened filename, set when lock is acquired.
};

/** @c basic_fstream_dyad specialization for @c char. */
using fstream_dyad = basic_fstream_dyad<char>;

/** @c basic_fstream_dyad specialization for @c wchar_t. */
using wfstream_dyad = basic_fstream_dyad<wchar_t>;

#if __cplusplus < 201103L  //----------------------------------------------------
template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const dyad_stream_core &core)
    : m_core (core)
{
    m_stream = new basic_fstream ();
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad ()
{
    m_core.init ();
    m_stream = new basic_fstream ();
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const char *filename,
                                                         std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename);
    m_stream = new basic_fstream (filename, mode);
    if ((m_stream != nullptr) && (*m_stream)) {
        if (m_core.is_dyad_producer ()
#if DYAD_HAS_STD_FSTREAM_FD
            && m_core.cmp_canonical_path_prefix (true, filename)
#endif  // DYAD_HAS_STD_FSTREAM_FD
        ) {
            DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM (*m_stream, m_core);
        }
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
bool basic_fstream_dyad<_CharT, _Traits>::is_open ()
{
    return ((m_stream != nullptr) && (m_stream->is_open ()));
}
#else  //-----------------------------------------------------------------------
template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const dyad_stream_core &core)
    : m_core (core)
{
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream ());
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad ()
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream ());
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const char *filename,
                                                         std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename);
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        if (m_core.is_dyad_producer ()) {
            DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM (*m_stream, m_core);
        }
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const string &filename,
                                                         std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename.c_str ());
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        if (m_core.is_dyad_producer ()) {
            DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM (*m_stream, m_core);
        }
        m_filename = filename;
    }
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (basic_fstream_dyad &&rhs)
    : m_stream (std::move (rhs.m_stream)),
      m_core (std::move (rhs.m_core)),
      m_filename (std::move (rhs.m_filename))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template <typename _CharT, typename _Traits>
template <typename _Path, typename _Require>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const _Path &filepath,
                                                         std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filepath.c_str ());
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream (filepath.c_str (), mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        if (m_core.is_dyad_producer ()) {
            DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM (*m_stream, m_core);
        }
        m_filename = std::string{filepath.c_str ()};
    }
}
#endif  // c++17 filesystem

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits> &basic_fstream_dyad<_CharT, _Traits>::operator= (
    basic_fstream_dyad &&rhs)
{
    m_stream = std::move (rhs.m_stream);
    m_core = std::move (rhs.m_core);
    m_filename = std::move (rhs.m_filename);
    return (*this);
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::~basic_fstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    if (m_stream->is_open ()) {
        if (m_core.is_dyad_producer ()) {
            if (m_core.chk_fsync_write ()) {
                fsync_fstream (*m_stream);
            }
            DYAD_UNLOCK_CPP_FSTREAM (*m_stream, m_core);
        }
#if __cplusplus < 201103L
        delete m_stream;
        m_stream = nullptr;
#else
        m_stream.reset ();
#endif
        m_core.close_sync (m_filename.c_str ());
    } else {
#if __cplusplus < 201103L
        delete m_stream;
        m_stream = nullptr;
#else
        m_stream.reset ();
#endif
    }
}

template <typename _CharT, typename _Traits>
void basic_fstream_dyad<_CharT, _Traits>::open (const string &filename,
                                                std::ios_base::openmode mode)
{
    open (filename.c_str (), mode);
}

template <typename _CharT, typename _Traits>
bool basic_fstream_dyad<_CharT, _Traits>::is_open () const
{
    return ((m_stream != nullptr) && (m_stream->is_open ()));
}

template <typename _CharT, typename _Traits>
void basic_fstream_dyad<_CharT, _Traits>::swap (basic_fstream_dyad &rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (*rhs.m_stream);
}
#endif  //-----------------------------------------------------------------------

template <typename _CharT, typename _Traits>
void basic_fstream_dyad<_CharT, _Traits>::open (const char *filename, std::ios_base::openmode mode)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_core.open_sync (filename);
    m_stream->open (filename, mode);
    if ((*m_stream)) {
        if (m_core.is_dyad_producer ()
#if DYAD_HAS_STD_FSTREAM_FD
            && m_core.cmp_canonical_path_prefix (true, filename)
#endif  // DYAD_HAS_STD_FSTREAM_FD
        ) {
            DYAD_EXCLUSIVE_LOCK_CPP_FSTREAM (*m_stream, m_core);
        }
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
void basic_fstream_dyad<_CharT, _Traits>::close ()
{
    if (m_stream == nullptr) {
        return;
    }
    if (m_core.is_dyad_producer ()) {
        if (m_core.chk_fsync_write ()) {
            fsync_fstream (*m_stream);
        }
        DYAD_UNLOCK_CPP_FSTREAM (*m_stream, m_core);
    }
    m_stream->close ();
    m_core.close_sync (m_filename.c_str ());
}

template <typename _CharT, typename _Traits>
std::filebuf *basic_fstream_dyad<_CharT, _Traits>::rdbuf () const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template <typename _CharT, typename _Traits>
std::basic_fstream<_CharT, _Traits> &basic_fstream_dyad<_CharT, _Traits>::get_stream ()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

template <typename _CharT, typename _Traits>
void basic_fstream_dyad<_CharT, _Traits>::init (const dyad_stream_core &core)
{
    m_core = core;
    m_core.set_initialized ();
    m_core.log_info ("Stream core state is set");
}

}  // end of namespace dyad
#endif  // DYAD_STREAM_DYAD_STREAM_API_HPP
