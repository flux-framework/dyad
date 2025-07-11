#ifndef DYAD_RESIDENCY_FCACHE_H
#define DYAD_RESIDENCY_FCACHE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/common/dyad_structures_int.h>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace dyad_residency
{

//=============================================================================
//                       Cache Block (cache line) for LRU
//=============================================================================

struct Simple_Block {
    std::string m_id;  // unique id, file name
    size_t m_size;     // byte size of block or file

    Simple_Block (const std::string& id, size_t sz = 0ul) : m_id (id), m_size (sz)
    {
    }
};

struct Ranked_Block {
    std::string m_id;         // unique id, file name
    unsigned int m_priority;  // priority
    size_t m_size;            // byte size of block or file

    Ranked_Block (const std::string& id, unsigned int priority, size_t sz = 0ul)
        : m_id (id), m_priority (priority), m_size (sz)
    {
    }
};

//=============================================================================
//                       Associative Cache Set
//=============================================================================

class Set_LRU
{
   protected:
    struct id {
    };
    struct priority {
    };
    typedef boost::multi_index_container<
        Simple_Block,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<id>,
                BOOST_MULTI_INDEX_MEMBER (Simple_Block, std::string, m_id)>,
            boost::multi_index::sequenced<boost::multi_index::tag<priority> > > >
        LRU_Blocks;

    typedef boost::multi_index::index<LRU_Blocks, id>::type id_idx_t;
    typedef boost::multi_index::index<LRU_Blocks, id>::type::iterator id_iterator_t;
    typedef boost::multi_index::index<LRU_Blocks, id>::type::const_iterator id_citerator_t;

    typedef boost::multi_index::index<LRU_Blocks, priority>::type priority_idx_t;
    typedef boost::multi_index::index<LRU_Blocks, priority>::type::iterator priority_iterator_t;
    typedef boost::multi_index::index<LRU_Blocks, priority>::type::const_iterator
        priority_citerator_t;

   protected:
    /** Cache set capacity (n-way in a set associative cache) in terms of the
     *  maximum number of blocks */
    unsigned int m_size;
    /// Total number of sets in the cache to which this set belongs
    unsigned int m_num_sets;
    /// Id of this set
    unsigned int m_id;

    /** Current access sequence number. This can be used to calculate the numbeer
     *  of accesses */
    unsigned int m_seqno;
    /** In case of cache miss counter reset, keep a record of the current seqno */
    unsigned int m_seqno0;
    /// Number of cache misses
    unsigned int m_num_miss;
    /// Level info of this cache. e,g., L1, or L2. This does not affect operation
    std::string m_level;

    /// Total amount of space being used in terms of bytes of data in the files cached
    size_t m_space_used;
    /// Max amount of space in terms of bytes that can be used to cache data files
    size_t m_space_max;
    /// DYAD context, used in DYAD APIs for locking, logging, profiling etc.
    const dyad_ctx_t* m_ctx;
    /// Cache set that can be looked up via the id or the priority of an item
    LRU_Blocks m_block_set;

    /// See if a block or item (e.g., filename) is present in this set
    virtual bool lookup (const std::string& fname, id_iterator_t& it);
    /// Check if the cache space is full. e.g., by the number of items or by the space used
    virtual bool is_full (size_t size_to_add) const;
    /// Evict a block or item out of the set
    virtual bool evict (void);
    /// Obtain the priority value to assign to a new block or item being added
    virtual unsigned int get_priority ();
    /// Add a new block or item into the set
    virtual void load_and_access (const std::string& fname, size_t fsize);
    /// Update the priority of the block or item being accessed
    virtual void access (id_iterator_t& it);

   public:
    Set_LRU (unsigned int sz, unsigned int n_sets, unsigned int id, const dyad_ctx_t* ctx)
        : m_size (sz),
          m_num_sets (n_sets),
          m_id (id),
          m_seqno (0u),
          m_seqno0 (0u),
          m_num_miss (0u),
          m_space_used (0ul),
          m_space_max (0ul),
          m_ctx (ctx)
    {
    }
    virtual ~Set_LRU ()
    {
    }

    /// Report the number of cache blocks/items in this set
    unsigned int size (void) const
    {
        return m_size;
    }
    /// Set the max space in bytes that can host file caches
    void set_space_max (size_t max_space);
    /// Report the max space in bytes that can host file caches
    size_t get_space_max () const;
    /// Report the space in bytes currently used by file caches
    size_t get_space_used () const;

    /// Report the number of cache accesses since the last reset or beginning
    unsigned int get_num_access (void) const
    {
        return m_seqno - m_seqno0;
    }
    /// Report the number of cache misses since the last reset or beginning
    unsigned int get_num_miss (void) const
    {
        return m_num_miss;
    }
    /// Reset the cache miss counter
    void reset_cnts (void)
    {
        m_seqno0 = m_seqno;  // Keep a record of when the miss counter is reset
        m_num_miss = 0u;     // Reset the miss counter
    }
    /// Report the level, which is simply a meta info that does not affect the operation
    std::string get_level (void) const
    {
        return m_level;
    }
    /// Set the level, which is simply a meta info that does not affect the operation
    void set_level (const std::string& level)
    {
        m_level = level;
    }

    /** Access by a block index, and returns true if hit. Otherwise false.
     *  And there must be a following access at the lower cache layer. */
    virtual bool access (const std::string& fname, size_t fsize);

    virtual std::ostream& print (std::ostream& os) const;
};

std::ostream& operator<< (std::ostream& os, const Set_LRU& sl);

class Set_MRU : public Set_LRU
{
   protected:
    using Set_LRU::id;
    using Set_LRU::LRU_Blocks;
    using Set_LRU::priority;

    using Set_LRU::id_citerator_t;
    using Set_LRU::id_idx_t;
    using Set_LRU::id_iterator_t;
    using Set_LRU::priority_citerator_t;
    using Set_LRU::priority_idx_t;
    using Set_LRU::priority_iterator_t;

    using Set_LRU::m_block_set;
    using Set_LRU::m_ctx;

    using Set_LRU::access;
    using Set_LRU::get_priority;
    using Set_LRU::is_full;
    using Set_LRU::load_and_access;
    using Set_LRU::lookup;

    virtual bool evict (void) override;

   public:
    using Set_LRU::get_level;
    using Set_LRU::get_num_access;
    using Set_LRU::get_num_miss;
    using Set_LRU::get_space_max;
    using Set_LRU::get_space_used;
    using Set_LRU::print;
    using Set_LRU::reset_cnts;
    using Set_LRU::set_level;
    using Set_LRU::set_space_max;
    using Set_LRU::size;

    Set_MRU (unsigned int sz, unsigned int n_sets, unsigned int id, const dyad_ctx_t* ctx)
        : Set_LRU (sz, n_sets, id, ctx)
    {
    }
    virtual ~Set_MRU () override
    {
    }
    virtual bool access (const std::string& fname, size_t fsize) override;
};

std::ostream& operator<< (std::ostream& os, const Set_MRU& sm);

class Set_Prioritized : public Set_LRU
{
   protected:
    struct id {
    };
    struct priority {
    };

    typedef boost::multi_index_container<
        Ranked_Block,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<id>,
                BOOST_MULTI_INDEX_MEMBER (Ranked_Block, std::string, m_id)>,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<priority>,
                BOOST_MULTI_INDEX_MEMBER (Ranked_Block, unsigned int, m_priority)> > >
        Prio_Blocks;

    typedef boost::multi_index::index<Prio_Blocks, id>::type id_idx_t;
    typedef boost::multi_index::index<Prio_Blocks, id>::type::iterator id_iterator_t;
    typedef boost::multi_index::index<Prio_Blocks, id>::type::const_iterator id_citerator_t;

    typedef boost::multi_index::index<Prio_Blocks, priority>::type priority_idx_t;
    typedef boost::multi_index::index<Prio_Blocks, priority>::type::iterator priority_iterator_t;
    typedef boost::multi_index::index<Prio_Blocks, priority>::type::const_iterator
        priority_citerator_t;

    using Set_LRU::is_full;
    using Set_LRU::m_ctx;

   private:
    using Set_LRU::access;
    using Set_LRU::lookup;

   protected:
    Prio_Blocks m_block_set;

    virtual bool lookup (const std::string& fname, id_iterator_t& it);
    virtual bool evict (void) override;
    virtual unsigned int get_priority () override;
    virtual void load_and_access (const std::string& fname, size_t fsize) override;
    virtual void access (id_iterator_t& it);

   public:
    using Set_LRU::get_level;
    using Set_LRU::get_num_access;
    using Set_LRU::get_num_miss;
    using Set_LRU::get_space_max;
    using Set_LRU::get_space_used;
    using Set_LRU::print;
    using Set_LRU::reset_cnts;
    using Set_LRU::set_level;
    using Set_LRU::set_space_max;
    using Set_LRU::size;

    Set_Prioritized (unsigned int sz, unsigned int n_sets, unsigned int id, const dyad_ctx_t* ctx)
        : Set_LRU (sz, n_sets, id, ctx)
    {
    }
    virtual ~Set_Prioritized () override
    {
    }
    virtual bool access (const std::string& fname, size_t fsize) override;
    virtual std::ostream& print (std::ostream& os) const override;
};

std::ostream& operator<< (std::ostream& os, const Set_Prioritized& sp);

//=============================================================================
//                          Set Associative  Cache
//=============================================================================

///  Set associative cache model
template <typename Set>
class Cache
{
   public:
    using Sets = typename std::vector<Set>;

   protected:
    unsigned int m_size;      ///< Capacity of cache in terms of the number of blocks
    unsigned int m_ways;      ///< For x-way set-associativity
    unsigned int m_num_sets;  ///< Number of sets
    unsigned int m_seed;      ///< Seed used to compute hash in determining cache set
    std::string m_level;      ///< Level of this cache. e,g., L2.
    const dyad_ctx_t* m_ctx;  ///< DYAD context

    Sets m_set;

    /**
     *  Choose the set to search for the item. It is based on the modulo operation on
     *  the hashing value of filename to search.
     */
    virtual unsigned int get_cache_set_id (const std::string& fname) const;

   public:
    Cache (unsigned int sz, unsigned int w, const dyad_ctx_t* ctx);
    virtual ~Cache ()
    {
    }

    /// Initialize. Currently only checks if the dyad context pointer is valid.
    virtual bool init ()
    {
        if (m_ctx == nullptr) {
            return false;
        }
        return true;
    }
    unsigned int size (void) const
    {
        return m_size;
    }
    unsigned int get_num_ways (void) const
    {
        return m_ways;
    }
    unsigned int get_num_sets (void) const
    {
        return m_num_sets;
    }
    unsigned int get_num_access (void) const;
    unsigned int get_num_miss (void) const;

    void reset_cnts (void);
    void set_seed (unsigned int seed)
    {
        m_seed = seed;
    }
    std::string get_level (void) const
    {
        return m_level;
    }
    void set_level (const std::string& level);

    /// Specify the max cache space for each set
    virtual bool set_space_max (const std::vector<size_t>& space_max);
    /// Report the max cache space defined for each set
    std::vector<size_t> get_space_max () const;
    /// Report the current cache space used by each set
    std::vector<size_t> get_space_used () const;

    /// Returns true if hit. Otherwise false.
    virtual bool access (const std::string& fname, size_t fsize = 0ul);

    virtual std::ostream& print (std::ostream& os) const;
};

template <typename Set>
std::ostream& operator<< (std::ostream& os, const Cache<Set>& cc);

/// Physically remove a file
int remove (const char* fname, const dyad_ctx_t* ctx);

}  // end of namespace dyad_residency

#include <dyad/service/residency/fcache_impl.hpp>

#endif  // DYAD_RESIDENCY_FCACHE_H
