#ifndef DYAD_RESIDENCY_FCACHE_IMPL_H
#define DYAD_RESIDENCY_FCACHE_IMPL_H
#include <dyad/utils/murmur3.h>
#include <dyad/residency/fcache.hpp>

namespace dyad_residency
{

//=============================================================================
//                              Cache
//=============================================================================

template <typename Set>
Cache<Set>::Cache (unsigned int sz, unsigned int w, const dyad_ctx_t* ctx)
    : m_size (sz), m_ways (w), m_seed (104729u), m_ctx (ctx)
{
    if (m_ways == 0) {  // fully associative
        m_num_sets = 1;
        m_ways = m_size;
    } else {
        m_num_sets = m_size / m_ways;
    }

    assert (m_size == m_ways * m_num_sets);

    m_set.reserve (m_num_sets);
    for (unsigned int i = 0u; i < m_num_sets; i++) {
        m_set.emplace_back (Set (m_ways, m_num_sets, i, ctx));
    }
}

template <typename Set>
unsigned int Cache<Set>::get_num_access (void) const
{
    unsigned int tot_num_access = 0u;
    for (unsigned int i = 0; i < m_num_sets; i++) {
        tot_num_access += m_set[i].get_num_access ();
    }
    return tot_num_access;
}

template <typename Set>
unsigned int Cache<Set>::get_num_miss (void) const
{
    unsigned int tot_num_miss = 0u;
    for (unsigned int i = 0; i < m_num_sets; i++) {
        tot_num_miss += m_set[i].get_num_miss ();
    }
    return tot_num_miss;
}

template <typename Set>
void Cache<Set>::reset_cnts (void)
{
    for (unsigned int i = 0; i < m_num_sets; i++) {
        m_set[i].reset_cnts ();
    }
}

template <typename Set>
unsigned int Cache<Set>::get_cache_set_id (const std::string& fname) const
{
#if 0
    return std::stoi (fname) % m_num_sets;
#else
    uint32_t hash[4] = {0u};  // Output for the hash

    if (fname.empty ()) {
        // TODO: report the exception
        return 0u;
    }
    const char* str = fname.c_str ();
    size_t str_len = fname.size ();
    char buf[256] = {'\0'};

    if (str_len < 128ul) {
        memcpy (buf, str, str_len);
        memset (buf + str_len, '@', 128ul - str_len);
        buf[128u] = '\0';
        str_len = 128ul;
        str = buf;
    }

    MurmurHash3_x64_128 (str, strlen (str), m_seed, hash);
    return (hash[0] ^ hash[1] ^ hash[2] ^ hash[3]) % m_num_sets;
#endif
}

template <typename Set>
bool Cache<Set>::access (const std::string& fname)
{
    const unsigned int set_id = get_cache_set_id (fname);

    return m_set[set_id].access (fname);
}

template <typename Set>
void Cache<Set>::set_level (const std::string& level)
{
    m_level = level;
    for (unsigned int i = 0; i < m_num_sets; i++) {
        m_set[i].set_level (level);
    }
}

template <typename Set>
std::ostream& Cache<Set>::print (std::ostream& os) const
{
    os << "==========================================" << std::endl;
    os << "size:  " << m_size << std::endl;
    os << "nAcc:  " << get_num_access () << std::endl;
    os << "nMiss: " << get_num_miss () << std::endl;

    typename Sets::const_iterator it = m_set.begin ();
    typename Sets::const_iterator itend = m_set.end ();
    for (unsigned int i = 0; it != itend; it++, i++) {
        os << "-------- set " << i << " ----------" << std::endl;
        os << *it << std::endl;
    }
    os << "==========================================" << std::endl;
    return os;
}

template <typename Set>
std::ostream& operator<< (std::ostream& os, const Cache<Set>& cc)
{
    return cc.print (os);
}

}  // end of namespace dyad_residency
#endif  // DYAD_RESIDENCY_FCACHE_IMPL_H
