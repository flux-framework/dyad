#include <dyad/utils/murmur3.h>
#include <dyad/residency/fcache.hpp>

#define DYAD_UTIL_LOGGER
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/utils/utils.h>
#include <fcntl.h>   // AT_FDCWD
#include <unistd.h>  // unlink
#include <cerrno>

namespace dyad_residency
{

int Set_LRU::remove (const char* fname) const
{
    dyad_rc_t rc = DYAD_RC_OK;
    int lock_fd = -1;
    struct flock exclusive_lock;

    if (fname == nullptr) {
        return DYAD_RC_BADFIO;
    }

    lock_fd = open (fname, O_RDWR);
    // DYAD_C_FUNCTION_UPDATE_INT ("lock_fd", lock_fd);
    if (lock_fd == -1) {
        DYAD_LOG_ERROR (m_ctx,
                        "Cannot evict file (%s) that does not exist or that with no permission "
                        "for!",
                        fname);
        return DYAD_RC_BADFIO;
    }

    rc = dyad_excl_flock (m_ctx, lock_fd, &exclusive_lock);
    if (DYAD_IS_ERROR (rc)) {
        goto failed_rm;
    }

    if (unlink (fname) != 0) {
        DYAD_LOG_INFO (m_ctx, "Error deleting file %s : %s", fname, strerror (errno));
        goto failed_rm;
    }

    dyad_release_flock (m_ctx, lock_fd, &exclusive_lock);
    close (lock_fd);

    return DYAD_RC_OK;

failed_rm:;
    dyad_release_flock (m_ctx, lock_fd, &exclusive_lock);
    close (lock_fd);

    return DYAD_RC_BADFIO;
}

//=============================================================================
//                          Associative Cache Set
//=============================================================================

// -------------------------- LRU ------------------------
bool Set_LRU::lookup (const std::string& fname, id_iterator_t& it)
{
    id_idx_t& index_id = boost::multi_index::get<id> (m_block_set);
    it = index_id.find (fname);
    return (it != index_id.end ());
}

void Set_LRU::evict (void)
{  // LRU
    if (m_block_set.size () == 0)
        return;
    priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    if (!index_priority.empty ()) {
        priority_iterator_t it = index_priority.begin ();
        const char* fname = it->m_id.c_str ();
        DYAD_LOG_INFO (NULL, "    %s evicts %s from set %u\n", m_level.c_str (), fname, m_id);
        // Physically remove the file
        remove (fname);
        index_priority.erase (it);
    }
}

void Set_LRU::load_and_access (const std::string& fname)
{
    m_num_miss++;

    DYAD_LOG_INFO (NULL, "    %s adds %s to set %u\n", m_level.c_str (), fname.c_str (), m_id);
    if (m_size == m_block_set.size ()) {
        evict ();
    }

    m_block_set.insert (Simple_Block (fname));
    m_seqno++;
}

void Set_LRU::access (id_iterator_t& it)
{
    Simple_Block blk = *it;
    m_block_set.erase (it);
    m_block_set.insert (blk);
    m_seqno++;
}

bool Set_LRU::access (const std::string& fname)
{
    id_iterator_t it;
    if (lookup (fname, it)) {  // hit
        DYAD_LOG_INFO (NULL,
                       "    %s reuses %s from set %u\n",
                       m_level.c_str (),
                       fname.c_str (),
                       m_id);
        access (it);
        return true;
    } else {  // miss
        load_and_access (fname);
        return false;
    }
}

unsigned int Set_LRU::get_priority ()
{
    return m_seqno;
}

std::ostream& Set_LRU::print (std::ostream& os) const
{
    os << "size         : " << m_size << std::endl;
    os << "num accesses : " << m_seqno << std::endl;
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

std::ostream& operator<< (std::ostream& os, const Set_LRU& cc)
{
    return cc.print (os);
}

// -------------------------- MRU ------------------------
void Set_MRU::evict (void)
{  // MRU
    if (m_block_set.size () == 0)
        return;
    priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    if (!index_priority.empty ()) {
        auto it = index_priority.end ();
        --it;
        const char* fname = it->m_id.c_str ();
        DYAD_LOG_INFO (NULL, "    %s evicts %s from set %u\n", m_level.c_str (), fname, m_id);
        // Physically remove the file
        remove (fname);
        index_priority.erase (it);
    }
}

bool Set_MRU::access (const std::string& fname)
{
    return Set_LRU::access (fname);
}

std::ostream& operator<< (std::ostream& os, const Set_MRU& cc)
{
    return cc.print (os);
}

// -------------------------- Prioritied ------------------------
bool Set_Prioritized::lookup (const std::string& fname, id_iterator_t& it)
{
    id_idx_t& index_id = boost::multi_index::get<id> (m_block_set);
    it = index_id.find (fname);
    return (it != index_id.end ());
}

void Set_Prioritized::evict (void)
{
    if (m_block_set.size () == 0)
        return;
    priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    if (!index_priority.empty ()) {
        priority_iterator_t it = index_priority.begin ();
        const char* fname = it->m_id.c_str ();
        DYAD_LOG_INFO (NULL, "    %s evicts %s from set %u\n", m_level.c_str (), fname, m_id);
        // Physically remove the file
        remove (fname);
        index_priority.erase (it);
    }
}

void Set_Prioritized::load_and_access (const std::string& fname)
{
    m_num_miss++;

    DYAD_LOG_INFO (NULL, "    %s adds %s to set %u\n", m_level.c_str (), fname.c_str (), m_id);
    if (m_size == m_block_set.size ()) {
        evict ();
    }

    m_block_set.insert (Ranked_Block (fname, get_priority ()));
    m_seqno++;
}

void Set_Prioritized::access (id_iterator_t& it)
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
    if (lookup (fname, it)) {  // hit
        DYAD_LOG_INFO (NULL,
                       "   %s reuses %s from set %u\n",
                       m_level.c_str (),
                       fname.c_str (),
                       m_id);
        access (it);
        return true;
    } else {  // miss
        load_and_access (fname);
        return false;
    }
}

unsigned int Set_Prioritized::get_priority ()
{
    return m_seqno;
}

std::ostream& Set_Prioritized::print (std::ostream& os) const
{
    os << "size         : " << m_size << std::endl;
    os << "num accesses : " << m_seqno << std::endl;
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

std::ostream& operator<< (std::ostream& os, const Set_Prioritized& cc)
{
    return cc.print (os);
}

}  // end of namespace dyad_residency
