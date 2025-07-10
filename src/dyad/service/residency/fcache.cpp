#include <dyad/utils/murmur3.h>
#include <dyad/service/residency/fcache.hpp>

#define DYAD_UTIL_LOGGER
#include <dyad/common/dyad_logging.h>
#include <dyad/common/dyad_profiler.h>
#include <dyad/common/dyad_rc.h>
#include <dyad/utils/utils.h>
#include <fcntl.h>
#include <unistd.h>  // unlink
#include <cerrno>

namespace dyad_residency
{

int remove (const char* fname, const dyad_ctx_t* ctx)
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
        DYAD_LOG_ERROR (ctx,
                        "Cannot evict file (%s) that does not exist or that with no permission "
                        "for!",
                        fname);
        return DYAD_RC_BADFIO;
    }

    rc = dyad_excl_flock (ctx, lock_fd, &exclusive_lock);
    if (DYAD_IS_ERROR (rc)) {
        goto failed_rm;
    }

    if (unlink (fname) != 0) {
        DYAD_LOG_INFO (ctx, "Error deleting file %s : %s", fname, strerror (errno));
        goto failed_rm;
    }

    dyad_release_flock (ctx, lock_fd, &exclusive_lock);
    close (lock_fd);

    return DYAD_RC_OK;

failed_rm:;
    dyad_release_flock (ctx, lock_fd, &exclusive_lock);
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

bool Set_LRU::is_full (size_t size_to_add) const
{
    return (m_size == m_block_set.size ()) || (m_space_used + size_to_add > m_space_max);
}

void Set_LRU::evict (void)
{  // LRU
    if (m_block_set.size () == 0)
        return;
    priority_idx_t& index_priority = boost::multi_index::get<priority> (m_block_set);
    if (!index_priority.empty ()) {
        priority_iterator_t it = index_priority.begin ();
        const char* fname = it->m_id.c_str ();
        DYAD_LOG_INFO (m_ctx, "    %s evicts %s from set %u\n", m_level.c_str (), fname, m_id);
        // Physically remove the file
        if (remove (fname, m_ctx) == DYAD_RC_OK) {
            if (m_space_used < it->m_size) {
                DYAD_LOG_ERROR (m_ctx,
                                "space_used (%ll)) is less than space being released (%lu)",
                                m_space_used,
                                it->m_size);
            }
            m_space_used -= it->m_size;
        } else {
            // TODO: This is to enable testing without actual file I/O.
            // Otherwise, load_and_access () should actually load the file in to the cache storage.
            m_space_used -= it->m_size;
        }
        index_priority.erase (it);
    }
}

unsigned int Set_LRU::get_priority ()
{
    return m_seqno;
}

void Set_LRU::load_and_access (const std::string& fname, size_t fsize)
{
    m_num_miss++;

    DYAD_LOG_INFO (m_ctx, "    %s adds %s to set %u\n", m_level.c_str (), fname.c_str (), m_id);
    while (is_full (fsize)) {
        evict ();
    }

    m_space_used += fsize;
    m_block_set.insert (Simple_Block (fname, fsize));
    m_seqno++;
}

void Set_LRU::access (id_iterator_t& it)
{
    Simple_Block blk = *it;
    m_block_set.erase (it);
    m_block_set.insert (blk);
    m_seqno++;
}

bool Set_LRU::access (const std::string& fname, size_t fsize)
{
    id_iterator_t it;
    if (lookup (fname, it)) {  // hit
        DYAD_LOG_INFO (m_ctx,
                       "    %s reuses %s from set %u\n",
                       m_level.c_str (),
                       fname.c_str (),
                       m_id);
        access (it);
        return true;
    } else {  // miss
        load_and_access (fname, fsize);
        return false;
    }
}

void Set_LRU::set_space_max (size_t max_space)
{
    m_space_max = max_space;
}

size_t Set_LRU::get_space_max () const
{
    return m_space_max;
}

size_t Set_LRU::get_space_used () const
{
    return m_space_used;
}

std::ostream& Set_LRU::print (std::ostream& os) const
{
    os << "size         : " << m_size << std::endl;
    os << "space_max    : " << m_space_max << std::endl;
    os << "space_used   : " << m_space_used << std::endl;
    os << "num accesses : " << m_seqno << std::endl;
    os << "num misses   : " << m_num_miss << std::endl;
    os << "items        : " << std::endl;

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
        DYAD_LOG_INFO (m_ctx, "    %s evicts %s from set %u\n", m_level.c_str (), fname, m_id);
        // Physically remove the file
        if (remove (fname, m_ctx) == DYAD_RC_OK) {
            if (m_space_used < it->m_size) {
                DYAD_LOG_ERROR (m_ctx,
                                "space_used (%ll)) is less than space being released (%lu)",
                                m_space_used,
                                it->m_size);
            }
            m_space_used -= it->m_size;
        }
        index_priority.erase (it);
    }
}

bool Set_MRU::access (const std::string& fname, size_t fsize)
{
    return Set_LRU::access (fname, fsize);
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
        DYAD_LOG_INFO (m_ctx, "    %s evicts %s from set %u\n", m_level.c_str (), fname, m_id);
        // Physically remove the file
        if (remove (fname, m_ctx) == DYAD_RC_OK) {
            if (m_space_used < it->m_size) {
                DYAD_LOG_ERROR (m_ctx,
                                "space_used (%ll)) is less than space being released (%lu)",
                                m_space_used,
                                it->m_size);
            }
            m_space_used -= it->m_size;
        }
        index_priority.erase (it);
    }
}

unsigned int Set_Prioritized::get_priority ()
{
    return m_seqno;
}

void Set_Prioritized::load_and_access (const std::string& fname, size_t fsize)
{
    m_num_miss++;

    DYAD_LOG_INFO (m_ctx, "    %s adds %s to set %u\n", m_level.c_str (), fname.c_str (), m_id);
    while (is_full (fsize)) {
        evict ();
    }

    m_space_used += fsize;
    m_block_set.insert (Ranked_Block (fname, get_priority (), fsize));
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

bool Set_Prioritized::access (const std::string& fname, size_t fsize)
{
    id_iterator_t it;
    if (lookup (fname, it)) {  // hit
        DYAD_LOG_INFO (m_ctx,
                       "   %s reuses %s from set %u\n",
                       m_level.c_str (),
                       fname.c_str (),
                       m_id);
        access (it);
        return true;
    } else {  // miss
        load_and_access (fname, fsize);
        return false;
    }
}

std::ostream& Set_Prioritized::print (std::ostream& os) const
{
    os << "size         : " << m_size << std::endl;
    os << "space_max    : " << m_space_max << std::endl;
    os << "space_used   : " << m_space_used << std::endl;
    os << "num accesses : " << m_seqno << std::endl;
    os << "num misses   : " << m_num_miss << std::endl;
    os << "items        :" << std::endl;

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
