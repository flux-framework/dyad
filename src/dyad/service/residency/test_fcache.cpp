#include <cstdlib>
#include <dyad/service/residency/dyad_cache.hpp>
#include <dyad/service/residency/fcache.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

namespace dyad_residency
{

enum Set_Type { Set_LRU_, Set_MRU_, Set_Prio_, Set_End_ };

struct file_item {
    std::string fname;
    size_t fsize;
    file_item (const std::string& fn, size_t fsz) : fname (fn), fsize (fsz)
    {
    }

    operator std::string const ()
    {
        return fname;
    }
};
typedef std::vector<file_item> Access;

template <template <typename S> typename C, typename S>
int run_cache (int seed,
               std::unique_ptr<C<S>>& cacheL1,
               std::unique_ptr<C<S>>& cacheL2,
               Access& acc)
{
    bool hitL1 = false;

    if (!cacheL1 || !cacheL2) {
        std::cerr << "Cannot allocate cache." << std::endl;
        return EXIT_FAILURE;
    }
    cacheL1->set_seed (seed + 104677u);
    cacheL1->set_level ("L1");
    cacheL2->set_seed (seed + 104681u);
    cacheL2->set_level ("  L2");
    cacheL1->set_space_max (std::vector<size_t> (cacheL1->get_num_sets (), 5ul));
    cacheL2->set_space_max (std::vector<size_t> (cacheL2->get_num_sets (), 5ul));

    std::cout << "L1 Cache size set to " << cacheL1->size () << " blocks" << std::endl;
    std::cout << "L2 Cache size set to " << cacheL2->size () << " blocks" << std::endl;

    std::cout << "Accessing block 1, 4, 2, and 3 in order" << std::endl;
    for (unsigned int i = 0; (i < cacheL1->size ()) && (i < acc.size ()); i++) {
        const auto& item = acc[i];
        hitL1 = cacheL1->access (item.fname, item.fsize);
        if (!hitL1)
            cacheL2->access (item.fname, item.fsize);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "Accessing block 5" << std::endl;
    hitL1 = cacheL1->access (acc[4].fname, acc[4].fsize);
    if (!hitL1)
        cacheL2->access (acc[4].fname, acc[4].fsize);

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "Accessing block 1, 4, 2, 3, 5, 5, and 3 in order" << std::endl;
    for (unsigned int i = 0; i < acc.size (); i++) {
        const auto& item = acc[i];
        hitL1 = cacheL1->access (item.fname, item.fsize);
        if (!hitL1)
            cacheL2->access (item.fname, item.fsize);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    return EXIT_SUCCESS;
}

template <template <typename S> typename C, typename S>
int run_dyad_cache (int seed, std::unique_ptr<C<S>>& cache, Access& acc, Access& owned)
{
    if (!cache) {
        std::cerr << "Cannot allocate cache." << std::endl;
        return EXIT_FAILURE;
    }
    cache->set_seed (seed + 104677u);
    cache->set_level ("Node-local SSD");
    cache->init (std::vector<std::string> (owned.begin (), owned.end ()));
    cache->set_space_max (std::vector<size_t> (cache->get_num_sets (), 4ul));

    std::cout << "DYAD Cache size set to " << cache->size () << " files" << std::endl;

    std::cout << "Accessing files 1, 4, 2, and 3 in order" << std::endl;
    for (unsigned int i = 0; (i < cache->size () - 1) && (i < acc.size ()); i++) {
        const auto& item = acc[i];
        cache->access (item.fname, item.fsize);
    }

    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << *cache << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "Accessing file 5" << std::endl;
    cache->access (acc[4].fname, acc[4].fsize);

    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << *cache << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "Accessing files 1, 4, 2, 3, 5, 5, and 3 in order" << std::endl;
    for (unsigned int i = 0; i < acc.size (); i++) {
        const auto& item = acc[i];
        cache->access (item.fname, item.fsize);
    }

    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << *cache << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    cache->del_perm ("11");
    cache->add_perm ("12");
    std::cout << "Accessing files 10, 11, and 12 in order where 10 and 12 are permanent resident "
                 "files"
              << std::endl;
    for (unsigned int i = 0; i < owned.size (); i++) {
        const auto& item = owned[i];
        cache->access (item.fname, item.fsize);
    }

    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << *cache << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    return EXIT_SUCCESS;
}

int cache_demo (const Set_Type st,
                int seed,
                unsigned sizeL1,
                unsigned waysL1,
                unsigned sizeL2,
                unsigned waysL2,
                Access& acc,
                const dyad_ctx_t* ctx)
{
    if (st == Set_LRU_) {
        auto cacheL1 = std::unique_ptr<Cache<Set_LRU>> (new Cache<Set_LRU> (sizeL1, waysL1, ctx));
        auto cacheL2 = std::unique_ptr<Cache<Set_LRU>> (new Cache<Set_LRU> (sizeL2, waysL2, ctx));
        return run_cache (seed, cacheL1, cacheL2, acc);
    } else if (st == Set_MRU_) {
        auto cacheL1 = std::unique_ptr<Cache<Set_MRU>> (new Cache<Set_MRU> (sizeL1, waysL1, ctx));
        auto cacheL2 = std::unique_ptr<Cache<Set_MRU>> (new Cache<Set_MRU> (sizeL2, waysL2, ctx));
        return run_cache (seed, cacheL1, cacheL2, acc);
    } else if (st == Set_Prio_) {
        auto cacheL1 = std::unique_ptr<Cache<Set_Prioritized>> (
            new Cache<Set_Prioritized> (sizeL1, waysL1, ctx));
        auto cacheL2 = std::unique_ptr<Cache<Set_Prioritized>> (
            new Cache<Set_Prioritized> (sizeL2, waysL2, ctx));
        return run_cache (seed, cacheL1, cacheL2, acc);
    }
    return EXIT_FAILURE;
}

int dyad_cache_demo (const Set_Type st,
                     int seed,
                     unsigned size,
                     Access& acc,
                     Access& owned,
                     const dyad_ctx_t* ctx)
{
    if (st == Set_LRU_) {
        auto cache = std::unique_ptr<DYAD_Cache<Set_LRU>> (new DYAD_Cache<Set_LRU> (size + 1, ctx));
        return run_dyad_cache (seed, cache, acc, owned);
    } else if (st == Set_MRU_) {
        auto cache = std::unique_ptr<DYAD_Cache<Set_MRU>> (new DYAD_Cache<Set_MRU> (size + 1, ctx));
        return run_dyad_cache (seed, cache, acc, owned);
    } else if (st == Set_Prio_) {
        auto cache = std::unique_ptr<DYAD_Cache<Set_Prioritized>> (
            new DYAD_Cache<Set_Prioritized> (size + 1, ctx));
        return run_dyad_cache (seed, cache, acc, owned);
    }
    return EXIT_FAILURE;
}

}  // namespace dyad_residency

int main (int argc, char** argv)
{
    using namespace dyad_residency;

    int seed = 0;
    Set_Type set_type = Set_LRU_;

    if (argc > 3) {
        return EXIT_FAILURE;
    }
    if (argc >= 2) {
        seed = atoi (argv[1]);
    }
    if (argc == 3) {
        int st = atoi (argv[2]);
        set_type = static_cast<Set_Type> (st % Set_End_);
    }

    dyad_ctx_t* ctx = (dyad_ctx_t*)malloc (sizeof (struct dyad_ctx));
    if (!ctx) {
        return EXIT_FAILURE;
    }

    Access acc;  // Access pattern in terms of the block index
    acc.emplace_back ("1", 1);
    acc.emplace_back ("4", 1);
    acc.emplace_back ("2", 1);
    acc.emplace_back ("3", 1);
    acc.emplace_back ("5", 1);
    acc.emplace_back ("5", 1);
    acc.emplace_back ("3", 1);

    cache_demo (set_type, seed, 4, 2, 8, 2, acc, ctx);

    std::cout << std::endl
              << std::endl
              << "################################" << std::endl
              << std::endl;

    Access owned;  // files that are locally produced
    owned.emplace_back ("10", 1);
    owned.emplace_back ("11", 1);
    dyad_cache_demo (set_type, seed, 4, acc, owned, ctx);

    return EXIT_SUCCESS;
}
