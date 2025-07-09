#ifndef DYAD_RESIDENCY_DYAD_CACHE_H
#define DYAD_RESIDENCY_DYAD_CACHE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/residency/fcache.hpp>

#include <unordered_set>

namespace dyad_residency
{

template <typename Set>
class DYAD_Cache : public Cache<Set>
{
   public:
    using Cache<Set>::Sets;
    using Cache<Set>::size;
    using Cache<Set>::get_num_sets;
    using Cache<Set>::get_num_access;
    using Cache<Set>::get_num_miss;
    using Cache<Set>::reset_cnts;
    using Cache<Set>::set_seed;
    using Cache<Set>::get_level;
    using Cache<Set>::set_level;
    using Cache<Set>::m_ctx;
    using PermSet = typename std::unordered_set<std::string>;

   protected:
    PermSet m_perm;  ///< Permanent resident set

    virtual unsigned int get_cache_set_id (const std::string& fname) const override;

   public:
    DYAD_Cache (unsigned int sz, const dyad_ctx_t* ctx) : Cache<Set> (sz, 0, ctx)
    {
    }

    virtual ~DYAD_Cache () override
    {
    }

    virtual bool init (const std::vector<std::string>& files)
    {
        m_perm.clear ();
        for (const auto& f : files) {
            m_perm.insert (f);
        }
        return Cache<Set>::init ();
    }

    virtual bool access (const std::string& fname) override;

    virtual std::ostream& print (std::ostream& os) const override;
};

template <typename Set>
unsigned int DYAD_Cache<Set>::get_cache_set_id (const std::string& fname) const
{
    return 0u;
}

template <typename Set>
bool DYAD_Cache<Set>::access (const std::string& fname)
{
    if (m_perm.count (fname) > 0u) {
        return true;
    }
    return Cache<Set>::access (fname);
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
