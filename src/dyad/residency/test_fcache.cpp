#include <cstdlib>
#include <dyad/residency/dyad_cache.hpp>
#include <dyad/residency/fcache.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

namespace dyad_residency
{

enum Set_Type { Set_LRU_, Set_MRU_, Set_Prio_, Set_End_ };

template <template <typename S> typename C, typename S>
int run_cache (int seed,
               std::unique_ptr<C<S>>& cacheL1,
               std::unique_ptr<C<S>>& cacheL2,
               std::vector<std::string>& acc)
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

    std::cout << "L1 Cache size set to " << cacheL1->size () << " blocks" << std::endl;
    std::cout << "L2 Cache size set to " << cacheL2->size () << " blocks" << std::endl;

    std::cout << "accessing block 1, 4, 2, and 3 in order" << std::endl;
    for (unsigned int i = 0; (i < cacheL1->size ()) && (i < acc.size ()); i++) {
        hitL1 = cacheL1->access (acc[i]);
        if (!hitL1)
            cacheL2->access (acc[i]);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing block 5" << std::endl;
    hitL1 = cacheL1->access (acc[4]);
    if (!hitL1)
        cacheL2->access (acc[4]);

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing block 1, 4, 2, 3, 5, 5, and 3 in order" << std::endl;
    for (unsigned int i = 0; i < acc.size (); i++) {
        hitL1 = cacheL1->access (acc[i]);
        if (!hitL1)
            cacheL2->access (acc[i]);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    return EXIT_SUCCESS;
}

template <template <typename S> typename C, typename S>
int run_dyad_cache (int seed,
                    std::unique_ptr<C<S>>& cache,
                    std::vector<std::string>& acc,
                    std::vector<std::string>& owned)
{
    if (!cache) {
        std::cerr << "Cannot allocate cache." << std::endl;
        return EXIT_FAILURE;
    }
    cache->set_seed (seed + 104677u);
    cache->set_level ("Node-local SSD");
    cache->init (owned);

    std::cout << "DYAD Cache size set to " << cache->size () << " files" << std::endl;

    std::cout << "accessing files 1, 4, 2, and 3 in order" << std::endl;
    for (unsigned int i = 0; (i < cache->size ()) && (i < acc.size ()); i++) {
        cache->access (acc[i]);
    }

    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << *cache << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing file 5" << std::endl;
    cache->access (acc[4]);

    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << *cache << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing files 1, 4, 2, 3, 5, 5, and 3 in order" << std::endl;
    for (unsigned int i = 0; i < acc.size (); i++) {
        cache->access (acc[i]);
    }

    std::cout << "-----------------------------------------------------" << std::endl;
    std::cout << *cache << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    cache->add_perm ("12");
    std::cout << "accessing files 10, 11, and 12 in order" << std::endl;
    for (unsigned int i = 0; i < owned.size (); i++) {
        cache->access (owned[i]);
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
                std::vector<std::string>& acc,
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
                     std::vector<std::string>& acc,
                     std::vector<std::string>& owned,
                     const dyad_ctx_t* ctx)
{
    if (st == Set_LRU_) {
        auto cache = std::unique_ptr<DYAD_Cache<Set_LRU>> (new DYAD_Cache<Set_LRU> (size, ctx));
        return run_dyad_cache (seed, cache, acc, owned);
    } else if (st == Set_MRU_) {
        auto cache = std::unique_ptr<DYAD_Cache<Set_MRU>> (new DYAD_Cache<Set_MRU> (size, ctx));
        return run_dyad_cache (seed, cache, acc, owned);
    } else if (st == Set_Prio_) {
        auto cache = std::unique_ptr<DYAD_Cache<Set_Prioritized>> (
            new DYAD_Cache<Set_Prioritized> (size, ctx));
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

    typedef std::vector<std::string> access;
    access acc;  // access pattern in terms of the block index
    acc.push_back ("1");
    acc.push_back ("4");
    acc.push_back ("2");
    acc.push_back ("3");
    acc.push_back ("5");
    acc.push_back ("5");
    acc.push_back ("3");

    cache_demo (set_type, seed, 4, 2, 8, 2, acc, ctx);

    std::cout << std::endl
              << std::endl
              << "################################" << std::endl
              << std::endl;

    access owned;  // files that are locally produced
    owned.push_back ("10");
    owned.push_back ("11");
    dyad_cache_demo (set_type, seed, 4, acc, owned, ctx);

    return EXIT_SUCCESS;
}
