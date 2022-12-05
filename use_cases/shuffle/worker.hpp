#ifndef WORKER_HPP
#define WORKER_HPP

#include <random>
#include <string>
#include <vector>

class Worker
{
   public:
    using idx_it_t = std::vector<size_t>::const_iterator;

   protected:
    using rn_gen_t = std::mt19937;
    unsigned int m_rank;
    unsigned int m_seed;
    rn_gen_t m_gen;
    std::vector<std::string> m_flist;
    std::vector<size_t> m_fidx;
    size_t m_begin;
    size_t m_end;

   public:
    Worker ();
    void set_rank (const int r);
    unsigned int get_rank () const
    {
        return m_rank;
    }
    void clear_seed ();
    void set_seed_by_clock ();
    void set_seed (const unsigned int s);
    void set_file_list (std::vector<std::string>&& flist);
    void set_file_list (const std::vector<std::string>& flist);
    void shuffle ();
    void split (int n_ranks);
    std::pair<size_t, size_t> get_range ();
    const std::vector<std::string>& get_file_list () const
    {
        return m_flist;
    }
    const std::vector<size_t>& get_indices () const
    {
        return m_fidx;
    }
    std::pair<idx_it_t, idx_it_t> get_iterator () const;
    std::string get_my_list_str () const;

   protected:
    void seed ();
    void set_file_list ();
};

#endif  // WORKER_HPP
