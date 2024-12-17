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
// Enable if _Path is a filesystem::path or experimental::filesystem::path
template <typename _Path,
          typename _Result = _Path,
          typename _Path2 = decltype (std::declval<_Path&> ().make_preferred ().filename ())>
using dyad_if_fs_path = std::enable_if_t<std::is_same_v<_Path, _Path2>, _Result>;
#endif  // c++17 filesystem

// https://stackoverflow.com/questions/676787/how-to-do-fsync-on-an-ofstream
template <typename _CharT, typename _Traits>
void fsync_ofstream (std::basic_ofstream<_CharT, _Traits>& os)
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
        fsync (static_cast<my_filebuf&> (*os.rdbuf ()).handle ());
    }
#else
    if (os.is_open ()) {
        os.flush ();
    }
#endif  // DYAD_HAS_STD_FSTREAM_FD
}

template <typename _CharT, typename _Traits>
void fsync_fstream (std::basic_fstream<_CharT, _Traits>& os)
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
        fsync (static_cast<my_filebuf&> (*os.rdbuf ()).handle ());
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
template <typename _CharT, typename _Traits>
void lock_exclusive_ofstream (std::basic_ofstream<_CharT, _Traits>& os,
                              const dyad_stream_core& core)
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
        int fd = static_cast<my_filebuf&> (*os.rdbuf ()).handle ();
        core.file_lock_exclusive (fd);
    }
}

template <typename _CharT, typename _Traits>
void lock_exclusive_fstream (std::basic_fstream<_CharT, _Traits>& os, const dyad_stream_core& core)
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
        int fd = static_cast<my_filebuf&> (*os.rdbuf ()).handle ();
        core.file_lock_exclusive (fd);
    }
}

template <typename _CharT, typename _Traits>
void lock_shared_ifstream (std::basic_ifstream<_CharT, _Traits>& is, const dyad_stream_core& core)
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
        int fd = static_cast<my_filebuf&> (*is.rdbuf ()).handle ();
        core.file_lock_shared (fd);
    }
}

template <typename _CharT, typename _Traits>
void lock_shared_fstream (std::basic_fstream<_CharT, _Traits>& is, const dyad_stream_core& core)
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
        int fd = static_cast<my_filebuf&> (*is.rdbuf ()).handle ();
        core.file_lock_shared (fd);
    }
}

template <typename _CharT, typename _Traits>
void unlock_ofstream (std::basic_ofstream<_CharT, _Traits>& os, const dyad_stream_core& core)
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
        int fd = static_cast<my_filebuf&> (*os.rdbuf ()).handle ();
        core.file_unlock (fd);
    }
}

template <typename _CharT, typename _Traits>
void unlock_ifstream (std::basic_ifstream<_CharT, _Traits>& is, const dyad_stream_core& core)
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
        int fd = static_cast<my_filebuf&> (*is.rdbuf ()).handle ();
        core.file_unlock (fd);
    }
}

template <typename _CharT, typename _Traits>
void unlock_fstream (std::basic_fstream<_CharT, _Traits>& os, const dyad_stream_core& core)
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
        int fd = static_cast<my_filebuf&> (*os.rdbuf ()).handle ();
        core.file_unlock (fd);
    }
}

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
//----------------------------------------------------------------------
#endif  // DYAD_HAS_STD_FSTREAM_FD
//----------------------------------------------------------------------

//=============================================================================
// basic_ifstream_dyad (std::basic_ifstream wrapper)
//=============================================================================

template <typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_ifstream_dyad
{
   public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_ifstream = typename std::basic_ifstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    basic_ifstream_dyad (const dyad_stream_core& core);
    basic_ifstream_dyad ();
    explicit basic_ifstream_dyad (const char* filename, ios_base::openmode mode = ios_base::in);
    ~basic_ifstream_dyad ();

    void open (const char* filename, ios_base::openmode mode = ios_base::in);

#if __cplusplus < 201103L
    bool is_open ();
#else
    explicit basic_ifstream_dyad (const string& filename, ios_base::openmode mode = ios_base::in);
    basic_ifstream_dyad (const basic_ifstream_dyad&) = delete;
    basic_ifstream_dyad (basic_ifstream_dyad&& rhs);
#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    template <typename _Path, typename _Require = dyad_if_fs_path<_Path> >
    basic_ifstream_dyad (const _Path& filepath, std::ios_base::openmode mode = std::ios_base::in);
#endif  // c++17 filesystem

    void open (const string& filename, ios_base::openmode mode = ios_base::in);
    bool is_open () const;

    basic_ifstream_dyad& operator= (basic_ifstream_dyad&& rhs);
    void swap (basic_ifstream_dyad& rhs);
#endif

    void close ();

    filebuf* rdbuf () const;

    basic_ifstream& get_stream ();

    void init (const dyad_stream_core& core);

    const dyad_stream_core& core () const
    {
        return m_core;
    }

   private:
    /// context info
    dyad_stream_core m_core;

#if __cplusplus < 201103L
    basic_ifstream m_stream;
#else
    std::unique_ptr<basic_ifstream> m_stream;
#endif

    std::string m_filename;
};

using ifstream_dyad = basic_ifstream_dyad<char>;
using wifstream_dyad = basic_ifstream_dyad<wchar_t>;

#if __cplusplus < 201103L  //----------------------------------------------------
template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const dyad_stream_core& core)
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
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const char* filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename);
    m_stream = new basic_ifstream (filename, mode);
    if ((m_stream != nullptr) && (*m_stream)) {
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
bool basic_ifstream_dyad<_CharT, _Traits>::is_open ()
{
    return ((m_stream != nullptr) && (m_stream->is_open ()));
}
#else  //-----------------------------------------------------------------------
template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const dyad_stream_core& core)
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
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const char* filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename);
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const string& filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename.c_str ());
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        m_filename = filename;
    }
}

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (basic_ifstream_dyad&& rhs)
    : m_stream (std::move (rhs.m_stream)), m_core (std::move (rhs.m_core))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template <typename _CharT, typename _Traits>
template <typename _Path, typename _Require>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (const _Path& filepath,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filepath.c_str ());
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream (filepath.c_str (), mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        m_filename = std::string{filepath.c_str ()};
    }
}
#endif  // c++17 filesystem

template <typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>& basic_ifstream_dyad<_CharT, _Traits>::operator= (
    basic_ifstream_dyad&& rhs)
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
void basic_ifstream_dyad<_CharT, _Traits>::open (const string& filename,
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
void basic_ifstream_dyad<_CharT, _Traits>::swap (basic_ifstream_dyad& rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (*rhs.m_stream);
}
#endif  //-----------------------------------------------------------------------

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::open (const char* filename, std::ios_base::openmode mode)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    DYAD_SHARED_LOCK_CPP_IFSTREAM (*m_stream, m_core);
    m_core.open_sync (filename);
    m_stream->open (filename, mode);
    if ((m_stream != nullptr) && (*m_stream)) {
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::close ()
{
    if (m_stream == nullptr) {
        return;
    }
    DYAD_UNLOCK_CPP_IFSTREAM (*m_stream, m_core);
    m_stream->close ();
}

template <typename _CharT, typename _Traits>
std::filebuf* basic_ifstream_dyad<_CharT, _Traits>::rdbuf () const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template <typename _CharT, typename _Traits>
std::basic_ifstream<_CharT, _Traits>& basic_ifstream_dyad<_CharT, _Traits>::get_stream ()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

template <typename _CharT, typename _Traits>
void basic_ifstream_dyad<_CharT, _Traits>::init (const dyad_stream_core& core)
{
    m_core = core;
    m_core.set_initialized ();
    m_core.log_info ("Stream core state is set");
}

//=============================================================================
// basic_ofstream_dyad (std::basic_ofstream wrapper)
//=============================================================================

template <typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_ofstream_dyad
{
   public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_ofstream = typename std::basic_ofstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    basic_ofstream_dyad (const dyad_stream_core& core);
    basic_ofstream_dyad ();
    explicit basic_ofstream_dyad (const char* filename, ios_base::openmode mode = ios_base::out);
    ~basic_ofstream_dyad ();

    void open (const char* filename, ios_base::openmode mode = ios_base::out);

#if __cplusplus < 201103L
    bool is_open ();
#else
    explicit basic_ofstream_dyad (const string& filename, ios_base::openmode mode = ios_base::out);
    basic_ofstream_dyad (const basic_ofstream_dyad&) = delete;
    basic_ofstream_dyad (basic_ofstream_dyad&& rhs);
#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    template <typename _Path, typename _Require = dyad_if_fs_path<_Path> >
    basic_ofstream_dyad (const _Path& filepath, std::ios_base::openmode mode = std::ios_base::out);
#endif  // c++18 filesystem

    void open (const string& filename, ios_base::openmode mode = ios_base::out);
    bool is_open () const;

    basic_ofstream_dyad& operator= (basic_ofstream_dyad&& rhs);
    void swap (basic_ofstream_dyad& rhs);
#endif

    void close ();

    filebuf* rdbuf () const;

    basic_ofstream& get_stream ();

    void init (const dyad_stream_core& core);

    const dyad_stream_core& core () const
    {
        return m_core;
    }

   private:
    /// context info
    dyad_stream_core m_core;

#if __cplusplus < 201103L
    basic_ofstream m_stream;
#else
    std::unique_ptr<basic_ofstream> m_stream;
#endif

    std::string m_filename;
};

using ofstream_dyad = basic_ofstream_dyad<char>;
using wofstream_dyad = basic_ofstream_dyad<wchar_t>;

#if __cplusplus < 201103L  //----------------------------------------------------
template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const dyad_stream_core& core)
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
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const char* filename,
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
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const dyad_stream_core& core)
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
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const char* filename,
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
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const string& filename,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_ofstream> (new basic_ofstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)
#if DYAD_HAS_STD_FSTREAM_FD
        && m_core.cmp_canonical_path_prefix (true, (const char* const)filename.c_str ())
#endif  // DYAD_HAS_STD_FSTREAM_FD
    ) {
        DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        m_filename = filename;
    }
}

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (basic_ofstream_dyad&& rhs)
    : m_stream (std::move (rhs.m_stream)), m_core (std::move (rhs.m_core))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template <typename _CharT, typename _Traits>
template <typename _Path, typename _Require>
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (const _Path& filepath,
                                                           std::ios_base::openmode mode)
{
    m_core.init ();
    m_stream = std::unique_ptr<basic_ofstream> (
        new basic_ofstream ((const char* const)filepath.c_str (), mode));

    if ((m_stream != nullptr) && (*m_stream)
#if DYAD_HAS_STD_FSTREAM_FD
        && m_core.cmp_canonical_path_prefix (true, filename.c_str ())
#endif  // DYAD_HAS_STD_FSTREAM_FD
    ) {
        DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        m_filename = std::string{filepath.c_str ()};
    }
}
#endif  // c++17 filesystem

template <typename _CharT, typename _Traits>
basic_ofstream_dyad<_CharT, _Traits>& basic_ofstream_dyad<_CharT, _Traits>::operator= (
    basic_ofstream_dyad&& rhs)
{
    m_stream = std::move (rhs.m_stream);
    m_core = std::move (rhs.m_core);
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
void basic_ofstream_dyad<_CharT, _Traits>::open (const string& filename,
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
void basic_ofstream_dyad<_CharT, _Traits>::swap (basic_ofstream_dyad& rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (*rhs.m_stream);
}
#endif  //-----------------------------------------------------------------------

template <typename _CharT, typename _Traits>
void basic_ofstream_dyad<_CharT, _Traits>::open (const char* filename, std::ios_base::openmode mode)
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
std::filebuf* basic_ofstream_dyad<_CharT, _Traits>::rdbuf () const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template <typename _CharT, typename _Traits>
std::basic_ofstream<_CharT, _Traits>& basic_ofstream_dyad<_CharT, _Traits>::get_stream ()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

template <typename _CharT, typename _Traits>
void basic_ofstream_dyad<_CharT, _Traits>::init (const dyad_stream_core& core)
{
    m_core = core;
    m_core.set_initialized ();
    m_core.log_info ("Stream core state is set");
}

//=============================================================================
// basic_fstream_dyad (std::basic_fstream wrapper)
//=============================================================================

template <typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_fstream_dyad
{
   public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_fstream = typename std::basic_fstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    basic_fstream_dyad (const dyad_stream_core& core);
    basic_fstream_dyad ();
    explicit basic_fstream_dyad (const char* filename,
                                 ios_base::openmode mode = ios_base::in | ios_base::out);
    ~basic_fstream_dyad ();

    void open (const char* filename, ios_base::openmode mode = ios_base::in | ios_base::out);

#if __cplusplus < 201103L
    bool is_open ();
#else
    explicit basic_fstream_dyad (const string& filename,
                                 ios_base::openmode mode = ios_base::in | ios_base::out);
    basic_fstream_dyad (const basic_fstream_dyad&) = delete;
    basic_fstream_dyad (basic_fstream_dyad&& rhs);
#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    template <typename _Path, typename _Require = dyad_if_fs_path<_Path> >
    basic_fstream_dyad (const _Path& filepath, std::ios_base::openmode mode = std::ios_base::out);
#endif  // c++18 filesystem

    void open (const string& filename, ios_base::openmode mode = ios_base::in | ios_base::out);
    bool is_open () const;

    basic_fstream_dyad& operator= (basic_fstream_dyad&& rhs);
    void swap (basic_fstream_dyad& rhs);
#endif

    void close ();

    filebuf* rdbuf () const;

    basic_fstream& get_stream ();

    void init (const dyad_stream_core& core);

    const dyad_stream_core& core () const
    {
        return m_core;
    }

   private:
    /// context info
    dyad_stream_core m_core;

#if __cplusplus < 201103L
    basic_fstream m_stream;
#else
    std::unique_ptr<basic_fstream> m_stream;
#endif

    std::string m_filename;
};

using fstream_dyad = basic_fstream_dyad<char>;
using wfstream_dyad = basic_fstream_dyad<wchar_t>;

#if __cplusplus < 201103L  //----------------------------------------------------
template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const dyad_stream_core& core)
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
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const char* filename,
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
            DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
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
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const dyad_stream_core& core)
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
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const char* filename,
                                                         std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename);
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        if (m_core.is_dyad_producer ()) {
            DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        }
        m_filename = std::string{filename};
    }
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const string& filename,
                                                         std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filename.c_str ());
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream (filename, mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        if (m_core.is_dyad_producer ()) {
            DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        }
        m_filename = filename;
    }
}

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (basic_fstream_dyad&& rhs)
    : m_stream (std::move (rhs.m_stream)), m_core (std::move (rhs.m_core))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template <typename _CharT, typename _Traits>
template <typename _Path, typename _Require>
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (const _Path& filepath,
                                                         std::ios_base::openmode mode)
{
    m_core.init ();
    m_core.open_sync (filepath.c_str ());
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream (filepath.c_str (), mode));
    if ((m_stream != nullptr) && (*m_stream)) {
        if (m_core.is_dyad_producer ()) {
            DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
        }
        m_filename = std::string{filepath.c_str ()};
    }
}
#endif  // c++17 filesystem

template <typename _CharT, typename _Traits>
basic_fstream_dyad<_CharT, _Traits>& basic_fstream_dyad<_CharT, _Traits>::operator= (
    basic_fstream_dyad&& rhs)
{
    m_stream = std::move (rhs.m_stream);
    m_core = std::move (rhs.m_core);
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
            DYAD_UNLOCK_CPP_OFSTREAM (*m_stream, m_core);
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
void basic_fstream_dyad<_CharT, _Traits>::open (const string& filename,
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
void basic_fstream_dyad<_CharT, _Traits>::swap (basic_fstream_dyad& rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (*rhs.m_stream);
}
#endif  //-----------------------------------------------------------------------

template <typename _CharT, typename _Traits>
void basic_fstream_dyad<_CharT, _Traits>::open (const char* filename, std::ios_base::openmode mode)
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
            DYAD_EXCLUSIVE_LOCK_CPP_OFSTREAM (*m_stream, m_core);
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
        DYAD_UNLOCK_CPP_OFSTREAM (*m_stream, m_core);
    }
    m_stream->close ();
    m_core.close_sync (m_filename.c_str ());
}

template <typename _CharT, typename _Traits>
std::filebuf* basic_fstream_dyad<_CharT, _Traits>::rdbuf () const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template <typename _CharT, typename _Traits>
std::basic_fstream<_CharT, _Traits>& basic_fstream_dyad<_CharT, _Traits>::get_stream ()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

template <typename _CharT, typename _Traits>
void basic_fstream_dyad<_CharT, _Traits>::init (const dyad_stream_core& core)
{
    m_core = core;
    m_core.set_initialized ();
    m_core.log_info ("Stream core state is set");
}

}  // end of namespace dyad
#endif  // DYAD_STREAM_DYAD_STREAM_API_HPP
