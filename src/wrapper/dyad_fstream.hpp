/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _DYAD_CPP_STREAM_HPP_
#define _DYAD_CPP_STREAM_HPP_
#include <string>
#include <fstream>
#include <memory>
#include <type_traits>

namespace dyad {

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    // Enable if _Path is a filesystem::path or experimental::filesystem::path
    template<typename _Path, typename _Result = _Path, typename _Path2
        = decltype(std::declval<_Path&>().make_preferred().filename())>
    using dyad_if_fs_path = std::enable_if_t<std::is_same_v<_Path, _Path2>, _Result>;
#endif // c++17 filesystem

//=============================================================================
// basic_ifstream_dyad (std::basic_ifstream wrapper)
//=============================================================================

template < typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_ifstream_dyad {
  public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_ifstream = typename std::basic_ifstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    basic_ifstream_dyad ();
    explicit basic_ifstream_dyad (const char* filename,
                                  ios_base::openmode mode = ios_base::in);
    ~basic_ifstream_dyad ();

    void open (const char* filename, ios_base::openmode mode = ios_base::in);

#if __cplusplus < 201103L
    bool is_open();
#else
    explicit basic_ifstream_dyad (const string& filename,
                                  ios_base::openmode mode = ios_base::in);
    basic_ifstream_dyad (const basic_ifstream_dyad&) = delete;
    basic_ifstream_dyad (basic_ifstream_dyad&& rhs);
  #if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    template<typename _Path, typename _Require = dyad_if_fs_path<_Path>>
    basic_ifstream_dyad (const _Path& filepath,
                         std::ios_base::openmode mode = std::ios_base::in);
  #endif // c++17 filesystem

    void open (const string& filename, ios_base::openmode mode = ios_base::in);
    bool is_open() const;

    basic_ifstream_dyad& operator= (basic_ifstream_dyad&& rhs);
    void swap (basic_ifstream_dyad& rhs);
#endif

    void close();

    filebuf* rdbuf() const;

    basic_ifstream& get_stream();

  private:

#if __cplusplus < 201103L
    basic_ifstream m_stream;
#else
    std::unique_ptr<basic_ifstream> m_stream;
#endif
};

using ifstream_dyad = basic_ifstream_dyad<char>;
using wifstream_dyad = basic_ifstream_dyad<wchar_t>;

#if __cplusplus < 201103L //----------------------------------------------------
template < typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad ()
{
    m_stream = new basic_ifstream ();
}

template < typename _CharT, typename _Traits>
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (
    const char* filename,
    std::ios_base::openmode mode)
{
    m_stream = new basic_ifstream (filename, mode);
}

template < typename _CharT, typename _Traits >
basic_ifstream_dyad<_CharT, _Traits>::~basic_ifstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    delete m_stream;
    m_stream = nullptr;
}

template < typename _CharT, typename _Traits >
bool basic_ifstream_dyad<_CharT, _Traits>::is_open ()
{
    return ((m_stream != nullptr) && (m_stream->is_open()));
}
#else //-----------------------------------------------------------------------
template < typename _CharT, typename _Traits >
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad ()
{
    m_stream = std::unique_ptr<basic_ifstream> (new basic_ifstream ());
}

template < typename _CharT, typename _Traits >
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (
    const char* filename,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_ifstream> (
                   new basic_ifstream (filename, mode));
}

template < typename _CharT, typename _Traits >
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (
    const string& filename,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_ifstream> (
                   new basic_ifstream (filename, mode));
}

template < typename _CharT, typename _Traits >
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (
    basic_ifstream_dyad&& rhs)
    : m_stream (std::move (rhs.m_stream))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template < typename _CharT, typename _Traits >
template < typename _Path, typename _Require >
basic_ifstream_dyad<_CharT, _Traits>::basic_ifstream_dyad (
    const _Path& filepath,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_ifstream> (
                   new basic_ifstream (filepath.c_str(), mode));
}
#endif // c++17 filesystem

template < typename _CharT, typename _Traits >
basic_ifstream_dyad<_CharT, _Traits>&
basic_ifstream_dyad<_CharT, _Traits>::operator= (
    basic_ifstream_dyad&& rhs)
{
    m_stream = std::move (rhs.m_stream);
    return (*this);
}

template < typename _CharT, typename _Traits >
basic_ifstream_dyad<_CharT, _Traits>::~basic_ifstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    m_stream.reset();
}

template < typename _CharT, typename _Traits >
void basic_ifstream_dyad<_CharT, _Traits>::open (
    const string& filename,
    std::ios_base::openmode mode)
{
    open (filename.c_str (), mode);
}

template < typename _CharT, typename _Traits >
bool basic_ifstream_dyad<_CharT, _Traits>::is_open () const
{
    return ((m_stream != nullptr) && (m_stream->is_open()));
}

template < typename _CharT, typename _Traits >
void basic_ifstream_dyad<_CharT, _Traits>::swap (
    basic_ifstream_dyad& rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (* rhs.m_stream);
}
#endif //-----------------------------------------------------------------------

template < typename _CharT, typename _Traits >
void basic_ifstream_dyad<_CharT, _Traits>::open (
    const char* filename,
    std::ios_base::openmode mode)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->open (filename, mode);
}

template < typename _CharT, typename _Traits >
void basic_ifstream_dyad<_CharT, _Traits>::close ()
{
    if (m_stream == nullptr) {
        return;
    }
    m_stream->close ();
}

template < typename _CharT, typename _Traits >
std::filebuf* basic_ifstream_dyad<_CharT, _Traits>::rdbuf() const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template < typename _CharT, typename _Traits >
std::basic_ifstream<_CharT, _Traits>&
basic_ifstream_dyad<_CharT, _Traits>::get_stream()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

//=============================================================================
// basic_ofstream_dyad (std::basic_ofstream wrapper)
//=============================================================================

template < typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_ofstream_dyad {
  public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_ofstream = typename std::basic_ofstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    basic_ofstream_dyad ();
    explicit basic_ofstream_dyad (const char* filename,
                                  ios_base::openmode mode = ios_base::out);
    ~basic_ofstream_dyad ();

    void open (const char* filename, ios_base::openmode mode = ios_base::out);

#if __cplusplus < 201103L
    bool is_open();
#else
    explicit basic_ofstream_dyad (const string& filename,
                                  ios_base::openmode mode = ios_base::out);
    basic_ofstream_dyad (const basic_ofstream_dyad&) = delete;
    basic_ofstream_dyad (basic_ofstream_dyad&& rhs);
  #if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    template<typename _Path, typename _Require = dyad_if_fs_path<_Path>>
    basic_ofstream_dyad (const _Path& filepath,
                         std::ios_base::openmode mode = std::ios_base::out);
  #endif // c++18 filesystem

    void open (const string& filename, ios_base::openmode mode = ios_base::out);
    bool is_open() const;

    basic_ofstream_dyad& operator= (basic_ofstream_dyad&& rhs);
    void swap (basic_ofstream_dyad& rhs);
#endif

    void close();

    filebuf* rdbuf() const;

    basic_ofstream& get_stream();

  private:

#if __cplusplus < 201103L
    basic_ofstream m_stream;
#else
    std::unique_ptr<basic_ofstream> m_stream;
#endif

};

using ofstream_dyad = basic_ofstream_dyad<char>;
using wofstream_dyad = basic_ofstream_dyad<wchar_t>;

#if __cplusplus < 201103L //----------------------------------------------------
template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad ()
{
    m_stream = new basic_ofstream ();
}

template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (
    const char* filename,
    std::ios_base::openmode mode)
{
    m_stream = new basic_ofstream (filename, mode);
}

template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::~basic_ofstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    delete m_stream;
    m_stream = nullptr;
}

template < typename _CharT, typename _Traits >
bool basic_ofstream_dyad<_CharT, _Traits>::is_open ()
{
    return ((m_stream != nullptr) && (m_stream->is_open()));
}
#else //-----------------------------------------------------------------------
template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad ()
{
    m_stream = std::unique_ptr<basic_ofstream> (new basic_ofstream ());
}

template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (
    const char* filename,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_ofstream> (
                   new basic_ofstream (filename, mode));
}

template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (
    const string& filename,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_ofstream> (
                   new basic_ofstream (filename, mode));
}

template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (
    basic_ofstream_dyad&& rhs)
    : m_stream (std::move (rhs.m_stream))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template < typename _CharT, typename _Traits >
template < typename _Path, typename _Require >
basic_ofstream_dyad<_CharT, _Traits>::basic_ofstream_dyad (
    const _Path& filepath,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_ofstream> (
                   new basic_ofstream (filepath.c_str(), mode));
}
#endif // c++17 filesystem

template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>&
basic_ofstream_dyad<_CharT, _Traits>::operator= (basic_ofstream_dyad&& rhs)
{
    m_stream = std::move (rhs.m_stream);
    return (*this);
}

template < typename _CharT, typename _Traits >
basic_ofstream_dyad<_CharT, _Traits>::~basic_ofstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    m_stream.reset();
}

template < typename _CharT, typename _Traits >
void basic_ofstream_dyad<_CharT, _Traits>::open (
    const string& filename,
    std::ios_base::openmode mode)
{
    open (filename.c_str (), mode);
}

template < typename _CharT, typename _Traits >
bool basic_ofstream_dyad<_CharT, _Traits>::is_open () const
{
    return ((m_stream != nullptr) && (m_stream->is_open()));
}

template < typename _CharT, typename _Traits >
void basic_ofstream_dyad<_CharT, _Traits>::swap (
    basic_ofstream_dyad& rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (* rhs.m_stream);
}
#endif //-----------------------------------------------------------------------

template < typename _CharT, typename _Traits >
void basic_ofstream_dyad<_CharT, _Traits>::open (
    const char* filename,
    std::ios_base::openmode mode)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->open (filename, mode);
}

template < typename _CharT, typename _Traits >
void basic_ofstream_dyad<_CharT, _Traits>::close ()
{
    if (m_stream == nullptr) {
        return;
    }
    m_stream->close ();
}

template < typename _CharT, typename _Traits >
std::filebuf* basic_ofstream_dyad<_CharT, _Traits>::rdbuf() const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template < typename _CharT, typename _Traits >
std::basic_ofstream<_CharT, _Traits>&
basic_ofstream_dyad<_CharT, _Traits>::get_stream()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

//=============================================================================
// basic_fstream_dyad (std::basic_fstream wrapper)
//=============================================================================

template < typename _CharT, typename _Traits = std::char_traits<_CharT> >
class basic_fstream_dyad {
  public:
    using ios_base = std::ios_base;
    using string = std::string;
    using basic_fstream = typename std::basic_fstream<_CharT, _Traits>;
    using filebuf = std::filebuf;

    basic_fstream_dyad ();
    explicit basic_fstream_dyad (const char* filename,
                 ios_base::openmode mode = ios_base::in | ios_base::out);
    ~basic_fstream_dyad ();

    void open (const char* filename,
               ios_base::openmode mode = ios_base::in | ios_base::out);

#if __cplusplus < 201103L
    bool is_open();
#else
    explicit basic_fstream_dyad (const string& filename,
                 ios_base::openmode mode = ios_base::in | ios_base::out);
    basic_fstream_dyad (const basic_fstream_dyad&) = delete;
    basic_fstream_dyad (basic_fstream_dyad&& rhs);
  #if (__cplusplus >= 201703L) && __has_include(<filesystem>)
    template<typename _Path, typename _Require = dyad_if_fs_path<_Path>>
    basic_fstream_dyad (const _Path& filepath,
                        std::ios_base::openmode mode = std::ios_base::out);
  #endif // c++18 filesystem

    void open (const string& filename,
               ios_base::openmode mode = ios_base::in | ios_base::out);
    bool is_open() const;

    basic_fstream_dyad& operator= (basic_fstream_dyad&& rhs);
    void swap (basic_fstream_dyad& rhs);
#endif

    void close();

    filebuf* rdbuf() const;

    basic_fstream& get_stream();

  private:

#if __cplusplus < 201103L
    basic_fstream m_stream;
#else
    std::unique_ptr<basic_fstream> m_stream;
#endif

};

using fstream_dyad = basic_fstream_dyad<char>;
using wfstream_dyad = basic_fstream_dyad<wchar_t>;

#if __cplusplus < 201103L //----------------------------------------------------
template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad ()
{
    m_stream = new basic_fstream ();
}

template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (
    const char* filename,
    std::ios_base::openmode mode)
{
    m_stream = new basic_fstream (filename, mode);
}

template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::~basic_fstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    delete m_stream;
    m_stream = nullptr;
}

template < typename _CharT, typename _Traits >
bool basic_fstream_dyad<_CharT, _Traits>::is_open ()
{
    return ((m_stream != nullptr) && (m_stream->is_open()));
}
#else //-----------------------------------------------------------------------
template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad ()
{
    m_stream = std::unique_ptr<basic_fstream> (new basic_fstream ());
}

template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (
    const char* filename,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_fstream> (
                   new basic_fstream (filename, mode));
}

template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (
    const string& filename,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_fstream> (
                   new basic_fstream (filename, mode));
}

template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (
    basic_fstream_dyad&& rhs)
    : m_stream (std::move (rhs.m_stream))
{
}

#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
template < typename _CharT, typename _Traits >
template < typename _Path, typename _Require >
basic_fstream_dyad<_CharT, _Traits>::basic_fstream_dyad (
    const _Path& filepath,
    std::ios_base::openmode mode)
{
    m_stream = std::unique_ptr<basic_fstream> (
                   new basic_fstream (filepath.c_str(), mode));
}
#endif // c++17 filesystem

template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>&
basic_fstream_dyad<_CharT, _Traits>::operator= (
    basic_fstream_dyad&& rhs)
{
    m_stream = std::move (rhs.m_stream);
    return (*this);
}

template < typename _CharT, typename _Traits >
basic_fstream_dyad<_CharT, _Traits>::~basic_fstream_dyad ()
{
    if (m_stream == nullptr) {
        return;
    }
    m_stream.reset();
}

template < typename _CharT, typename _Traits >
void basic_fstream_dyad<_CharT, _Traits>::open (
    const string& filename,
    std::ios_base::openmode mode)
{
    open (filename.c_str (), mode);
}

template < typename _CharT, typename _Traits >
bool basic_fstream_dyad<_CharT, _Traits>::is_open () const
{
    return ((m_stream != nullptr) && (m_stream->is_open()));
}

template < typename _CharT, typename _Traits >
void basic_fstream_dyad<_CharT, _Traits>::swap (basic_fstream_dyad& rhs)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->swap (* rhs.m_stream);
}
#endif //-----------------------------------------------------------------------

template < typename _CharT, typename _Traits >
void basic_fstream_dyad<_CharT, _Traits>::open (
    const   char* filename,
    std::ios_base::openmode mode)
{
    if (m_stream == nullptr) {
        // TODO: set fail bit if nullptr
        return;
    }
    m_stream->open (filename, mode);
}

template < typename _CharT, typename _Traits >
void basic_fstream_dyad<_CharT, _Traits>::close ()
{
    if (m_stream == nullptr) {
        return;
    }
    m_stream->close ();
}

template < typename _CharT, typename _Traits >
std::filebuf* basic_fstream_dyad<_CharT, _Traits>::rdbuf() const
{
    if (m_stream == nullptr) {
        return nullptr;
    }
    return m_stream->rdbuf ();
}

template < typename _CharT, typename _Traits >
std::basic_fstream<_CharT, _Traits>&
basic_fstream_dyad<_CharT, _Traits>::get_stream()
{
    if (m_stream == nullptr) {
        // TODO: throw
    }
    return *m_stream;
}

} // end of namespace dyad
#endif // _DYAD_CPP_STREAM_HPP_
