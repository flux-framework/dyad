#ifndef DYAD_RESIDENCY_DYAD_CACHE_H
#define DYAD_RESIDENCY_DYAD_CACHE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/service/residency/fcache.hpp>

#include <unordered_set>

namespace dyad_residency
{

template <typename Set>
class DYAD_Cache : public Cache<Set>
{
   public:
    using Cache<Set>::Sets;
    using Cache<Set>::size;
    using Cache<Set>::get_num_ways;
    using Cache<Set>::get_num_sets;
    using Cache<Set>::get_num_access;
    using Cache<Set>::get_num_miss;
    using Cache<Set>::reset_cnts;
    using Cache<Set>::set_seed;
    using Cache<Set>::get_level;
    using Cache<Set>::set_level;
    using Cache<Set>::set_space_max;
    using Cache<Set>::get_space_max;
    using Cache<Set>::get_space_used;
    using PermSet = typename std::unordered_set<std::string>;

   protected:
    using Cache<Set>::m_ctx;
    using Cache<Set>::m_set;
    PermSet m_perm;  ///< Permanent resident set

    virtual unsigned int get_cache_set_id (const std::string& fname) const override;

   public:
    DYAD_Cache (unsigned int sz, const dyad_ctx_t* ctx) : Cache<Set> (sz, 0, ctx)
    {
    }

    virtual ~DYAD_Cache () override
    {
    }

    /// Initialize with a set of permant resident file names
    virtual bool init (const std::vector<std::string>& files)
    {
        m_perm.clear ();
        for (const auto& f : files) {
            m_perm.insert (f);
        }
        return Cache<Set>::init ();
    }

    /// Allow adding a permant resident file
    virtual bool add_perm (const std::string& f)
    {
        auto res = m_perm.emplace (f);
        return res.second;
    }

    /// Allow removing a permant resident file
    virtual bool del_perm (const std::string& f)
    {
        remove (f.c_str (), m_ctx);
        return (m_perm.erase (f) > 0ul);
    }

    virtual bool access (const std::string& fname, size_t fsize = 0ul) override;

    virtual std::ostream& print (std::ostream& os) const override;
};

/**
 * Hard-coded to use only one set that is fully-associative.
 */
template <typename Set>
unsigned int DYAD_Cache<Set>::get_cache_set_id (const std::string& fname) const
{
    return 0u;
}

template <typename Set>
bool DYAD_Cache<Set>::access (const std::string& fname, size_t fsize)
{
    if (m_perm.count (fname) > 0u) {
        return true;
    }
    return Cache<Set>::access (fname, fsize);
}

template <typename Set>
std::ostream& DYAD_Cache<Set>::print (std::ostream& os) const
{
    Cache<Set>::print (os);
    return os;
}

template <typename Set>
std::ostream& operator<< (std::ostream& os, const DYAD_Cache<Set>& cc)
{
    return cc.print (os);
}

}  // end of namespace dyad_residency

#endif  // DYAD_RESIDENCY_DYAD_CACHE_H
