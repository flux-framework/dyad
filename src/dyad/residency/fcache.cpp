#include <dyad/residency/fcache.hpp>
#include <dyad/utils/murmur3.h>

#define DYAD_UTIL_LOGGER
#include <dyad/common/dyad_logging.h>


namespace dyad_residency {

//=============================================================================
//                          Associative Cache Set
//=============================================================================

// -------------------------- LRU ------------------------
bool Set_LRU::lookup (const std::string& fname, id_iterator_t &it)
{
    id_idx_t& index_id = boost::multi_index::get<id> (m_block_set);
    it = index_id.find (fname);
    return (it != index_id.end ());
}

void Set_LRU::evict (void)
{ // LRU
    if (m_block_set.size () == 0) return;
    priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    if (!index_priority.empty ()) {
        priority_iterator_t it = index_priority.begin ();
        DYAD_LOG_INFO (NULL, "    %s evicts %s from set %u\n", \
                       m_level.c_str (), it->m_id.c_str (), m_id);
        index_priority.erase (it);
    }
}

void Set_LRU::load_and_access (const std::string& fname)
{
    m_num_miss++;

    DYAD_LOG_INFO (NULL, "    %s adds %s to set %u\n", \
                   m_level.c_str (), fname.c_str (), m_id);
    if (m_size == m_block_set.size ()) {
        evict ();
    }

    m_block_set.insert (Simple_Block (fname));
    m_seqno++;
}

void Set_LRU::access (id_iterator_t &it)
{
    Simple_Block blk = *it;
    m_block_set.erase (it);
    m_block_set.insert (blk);
    m_seqno++;
}

bool Set_LRU::access (const std::string& fname)
{
    id_iterator_t it;
    if (lookup (fname, it)) { // hit
        DYAD_LOG_INFO (NULL, "    %s reuses %s from set %u\n", \
                       m_level.c_str (), fname.c_str (), m_id);
        access (it);
        return true;
    } else { // miss
        load_and_access (fname);
        return false;
    }
}

unsigned int Set_LRU::get_priority ()
{
    return m_seqno;
}

std::ostream& Set_LRU::print (std::ostream &os) const
{
    os << "size         : " << m_size << std::endl;
    os << "num accesses : " << m_seqno<< std::endl;
    os << "num misses   : " << m_num_miss << std::endl;
    os << "blkId        : " << std::endl;

    const priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    priority_citerator_t it = index_priority.begin ();
    priority_citerator_t itend = index_priority.end ();

    for (; it != itend; it++) {
        os << it->m_id << std::endl;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Set_LRU & cc)
{
    return cc.print (os);
}

// -------------------------- MRU ------------------------
void Set_MRU::evict (void)
{ // MRU
    if (m_block_set.size () == 0) return;
    priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    if (!index_priority.empty ()) {
        auto it = index_priority.end (); --it;
        DYAD_LOG_INFO (NULL, "    %s evicts %s from set %u\n", \
                       m_level.c_str (), it->m_id.c_str (), m_id);
        index_priority.erase (it);
    }
}

bool Set_MRU::access (const std::string& fname)
{
    return Set_LRU::access (fname);
}

std::ostream& operator<<(std::ostream& os, const Set_MRU & cc)
{
    return cc.print (os);
}



// -------------------------- Prioritied ------------------------
bool Set_Prioritized::lookup (const std::string& fname, id_iterator_t &it)
{
    id_idx_t& index_id = boost::multi_index::get<id> (m_block_set);
    it = index_id.find (fname);
    return (it != index_id.end ());
}

void Set_Prioritized::evict (void)
{
    if (m_block_set.size () == 0) return;
    priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    priority_iterator_t it = index_priority.begin ();
    DYAD_LOG_INFO (NULL, "    %s evicts %s from set %u\n", \
                   m_level.c_str (), it->m_id.c_str (), m_id);
    index_priority.erase (it);
}

void Set_Prioritized::load_and_access (const std::string& fname)
{
    m_num_miss++;

    DYAD_LOG_INFO (NULL, "    %s adds %s to set %u\n", \
                   m_level.c_str (), fname.c_str (), m_id);
    if (m_size == m_block_set.size ()) {
        evict ();
    }

    m_block_set.insert (Ranked_Block (fname, get_priority ()));
    m_seqno++;
}

void Set_Prioritized::access (id_iterator_t &it)
{
    Ranked_Block blk = *it;
    // reassigning the priority
    blk.m_priority = get_priority ();
    m_block_set.erase (it);
    m_block_set.insert (blk);
    m_seqno++;
}

bool Set_Prioritized::access (const std::string& fname)
{
    id_iterator_t it;
    if (lookup (fname, it)) { // hit
        DYAD_LOG_INFO (NULL, "   %s reuses %s from set %u\n", \
                       m_level.c_str (), fname.c_str (), m_id);
        access (it);
        return true;
    } else { // miss
        load_and_access (fname);
        return false;
    }
}

unsigned int Set_Prioritized::get_priority ()
{
    return m_seqno;
}

std::ostream& Set_Prioritized::print (std::ostream &os) const
{
    os << "size         : " << m_size << std::endl;
    os << "num accesses : " << m_seqno<< std::endl;
    os << "num misses   : " << m_num_miss << std::endl;
    os << "priority blkId:" << std::endl;

    const priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    priority_citerator_t it = index_priority.begin ();
    priority_citerator_t itend = index_priority.end ();

    for (; it != itend; it++) {
        os << it->m_priority << ", " << it->m_id << std::endl;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Set_Prioritized& cc)
{
    return cc.print (os);
}

} // end of namespace dyad_residency
