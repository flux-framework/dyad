#ifndef DYAD_RESIDENCY_FCACHE_H
#define DYAD_RESIDENCY_FCACHE_H

#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>

#include <iostream>
#include <vector>
#include <cassert>
#include <string>


namespace dyad_residency {

//=============================================================================
//                       Cache Block (cache line) for LRU
//=============================================================================

struct Simple_Block
{
    std::string m_id;        // unique id, file name

    Simple_Block (const std::string& id)
    : m_id (id) {}
};

struct Ranked_Block
{
    std::string m_id;        // unique id, file name
    unsigned int m_priority; // priority

    Ranked_Block (const std::string& id, unsigned int priority)
    : m_id (id), m_priority (priority) {}
};


//=============================================================================
//                       Associative Cache Set
//=============================================================================

class Set_LRU
{
  protected:
    struct id{};
    struct priority{};
    typedef boost::multi_index_container<
        Simple_Block,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<boost::multi_index::tag<id>,
                                               BOOST_MULTI_INDEX_MEMBER(Simple_Block, std::string, m_id)>,
            boost::multi_index::sequenced<boost::multi_index::tag<priority> >
        >
    > LRU_Blocks;

    typedef boost::multi_index::index<LRU_Blocks,id>::type id_idx_t;
    typedef boost::multi_index::index<LRU_Blocks,id>::type::iterator  id_iterator_t;
    typedef boost::multi_index::index<LRU_Blocks,id>::type::const_iterator  id_citerator_t;

    typedef boost::multi_index::index<LRU_Blocks,priority>::type priority_idx_t;
    typedef boost::multi_index::index<LRU_Blocks,priority>::type::iterator  priority_iterator_t;
    typedef boost::multi_index::index<LRU_Blocks,priority>::type::const_iterator  priority_citerator_t;

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

    LRU_Blocks m_block_set;

    virtual bool lookup (const std::string& fname, id_iterator_t &it);
    virtual void evict (void);
    virtual void access (id_iterator_t &it);
    virtual void load_and_access (const std::string& fname);
    virtual unsigned int get_priority ();

  public:
    Set_LRU(unsigned int sz, unsigned int n_sets, unsigned int id)
    : m_size (sz), m_num_sets (n_sets), m_id (id),
      m_seqno (0u), m_seqno0 (0u), m_num_miss (0u) {}
    virtual ~Set_LRU () {}

    unsigned int size (void) const { return m_size; }
    unsigned int get_num_access (void) const { return m_seqno - m_seqno0; }
    unsigned int get_num_miss (void) const { return m_num_miss; }
    void reset_cnts (void) { m_seqno0 = m_seqno; m_num_miss = 0u; }
    std::string get_level (void) const { return m_level; }
    void set_level (const std::string& level) { m_level = level; }

    /** Access by a block index, and returns true if hit. Otherwise false.
     *  And there must be a following access at the lower cache layer. */
    virtual bool access (const std::string& fname);

    virtual std::ostream& print (std::ostream &os) const;
};

std::ostream& operator<<(std::ostream& os, const Set_LRU & sl);

class Set_MRU : public Set_LRU
{
  protected:
    using Set_LRU::id;
    using Set_LRU::priority;
    using Set_LRU::LRU_Blocks;
    using Set_LRU::id_idx_t;
    using Set_LRU::id_iterator_t;
    using Set_LRU::id_citerator_t;
    using Set_LRU::priority_idx_t;
    using Set_LRU::priority_iterator_t;
    using Set_LRU::priority_citerator_t;
    using Set_LRU::m_block_set;

    using Set_LRU::lookup;
    using Set_LRU::access;
    using Set_LRU::load_and_access;
    using Set_LRU::get_priority;

    virtual void evict (void) override;

  public:
    Set_MRU (unsigned int sz, unsigned int n_sets, unsigned int id)
    : Set_LRU (sz, n_sets, id) {}
    virtual ~Set_MRU () override {}
    virtual bool access (const std::string& fname) override;

    using Set_LRU::print;
};

std::ostream& operator<<(std::ostream& os, const Set_MRU & sm);


class Set_Prioritized : public Set_LRU
{
  protected:
    struct id{};
    struct priority{};

    typedef boost::multi_index_container<
        Ranked_Block,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<boost::multi_index::tag<id>,
                                               BOOST_MULTI_INDEX_MEMBER(Ranked_Block, std::string, m_id)>,
            boost::multi_index::ordered_non_unique<boost::multi_index::tag<priority>,
                                               BOOST_MULTI_INDEX_MEMBER(Ranked_Block, unsigned int, m_priority)>
        >
    > Prio_Blocks;

    typedef boost::multi_index::index<Prio_Blocks,id>::type id_idx_t;
    typedef boost::multi_index::index<Prio_Blocks,id>::type::iterator  id_iterator_t;
    typedef boost::multi_index::index<Prio_Blocks,id>::type::const_iterator  id_citerator_t;

    typedef boost::multi_index::index<Prio_Blocks,priority>::type priority_idx_t;
    typedef boost::multi_index::index<Prio_Blocks,priority>::type::iterator  priority_iterator_t;
    typedef boost::multi_index::index<Prio_Blocks,priority>::type::const_iterator  priority_citerator_t;

  private:
    using Set_LRU::lookup;
    using Set_LRU::access;

  protected:
    Prio_Blocks m_block_set;

    virtual bool lookup (const std::string& fname, id_iterator_t &it);
    virtual void evict (void) override;
    virtual void access (id_iterator_t &it);
    virtual void load_and_access (const std::string& fname) override;
    virtual unsigned int get_priority () override;

  public:
    Set_Prioritized (unsigned int sz, unsigned int n_sets, unsigned int id)
    : Set_LRU (sz, n_sets, id) {}
    virtual ~Set_Prioritized () override {}
    virtual bool access (const std::string& fname) override;
    virtual std::ostream& print (std::ostream &os) const override;
};

std::ostream& operator<<(std::ostream& os, const Set_Prioritized & sp);

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
    unsigned int m_size;     ///< Capacity of cache in terms of the number of blocks
    unsigned int m_ways;     ///< For x-way set-associativity
    unsigned int m_num_sets; ///< Number of sets
    unsigned int m_seed;     ///< Seed used to compute hash in determining cache set
    std::string m_level;     ///< Level of this cache. e,g., L2.

    Sets m_set;

    unsigned int get_cache_set_id (const std::string& fname) const;

  public:
    Cache (unsigned int sz, unsigned int w);
    virtual ~Cache () {}

    unsigned int size (void) const { return m_size; }
    unsigned int get_num_ways (void) const { return m_ways; }
    unsigned int get_num_sets (void) const { return m_num_sets; }
    unsigned int get_num_access (void) const;
    unsigned int get_num_miss (void) const;

    void reset_cnts (void);
    void set_seed (unsigned int seed) { m_seed = seed; }
    std::string get_level (void) const { return m_level; }
    void set_level (const std::string& level);

    /// Returns true if hit. Otherwise false.
    virtual bool access (const std::string& fname);

    std::ostream& print (std::ostream &os) const;
};

template <typename Set>
std::ostream& operator<< (std::ostream& os, const Cache<Set> & cc);


} // end of namespace dyad_residency

#include <dyad/residency/fcache_impl.hpp>

#endif // DYAD_RESIDENCY_FCACHE_H
