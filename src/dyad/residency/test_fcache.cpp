#include <dyad/residency/fcache.hpp>
#include <string>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <type_traits>

namespace dyad_residency
{

enum Set_Type {Set_LRU_, Set_MRU_, Set_Prio_, Set_End_};


template<Set_Type V>
using st = std::integral_constant<Set_Type, V>;

std::unique_ptr<Cache<Set_LRU>> make_cache(st<Set_LRU_>, unsigned size, unsigned ways) {
    return std::unique_ptr<Cache<Set_LRU>>(new Cache<Set_LRU>(size, ways));
}
std::unique_ptr<Cache<Set_MRU>> make_cache(st<Set_MRU_>, unsigned size, unsigned ways) {
    return std::unique_ptr<Cache<Set_MRU>>(new Cache<Set_MRU>(size, ways));
}
std::unique_ptr<Cache<Set_Prioritized>> make_cache(st<Set_Prio_>, unsigned size, unsigned ways) {
    return std::unique_ptr<Cache<Set_Prioritized>>(new Cache<Set_Prioritized>(size, ways));
}

template< template<typename S> typename C, typename S>
int run_cache (int seed,
               std::unique_ptr<C<S>>& cacheL1,
               std::unique_ptr<C<S>>& cacheL2,
               const std::vector<std::string>& acc)
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
        if (!hitL1) cacheL2->access (acc[i]);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing block 5" << std::endl;
    hitL1 = cacheL1->access (acc[4]);
    if (!hitL1) cacheL2->access (acc[4]);

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing block 1, 4, 2, 3, 5, 5, and 3 in order" << std::endl;
    for (unsigned int i = 0; i < acc.size (); i++) {
        hitL1 = cacheL1->access (acc[i]);
        if (!hitL1) cacheL2->access (acc[i]);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << *cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << *cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    return EXIT_SUCCESS;
}

int cache_demo (const Set_Type st, int seed,
                unsigned sizeL1, unsigned waysL1,
                unsigned sizeL2, unsigned waysL2,
                const std::vector<std::string>& acc)
{
    if (st == Set_LRU_) {
        auto cacheL1 = std::unique_ptr<Cache<Set_LRU>>(new Cache<Set_LRU> (sizeL1, waysL1));
        auto cacheL2 = std::unique_ptr<Cache<Set_LRU>>(new Cache<Set_LRU> (sizeL2, waysL2));
        return run_cache (seed, cacheL1, cacheL2, acc);
    } else if (st == Set_MRU_) {
        auto cacheL1 = std::unique_ptr<Cache<Set_MRU>>(new Cache<Set_MRU> (sizeL1, waysL1));
        auto cacheL2 = std::unique_ptr<Cache<Set_MRU>>(new Cache<Set_MRU> (sizeL2, waysL2));
        return run_cache (seed, cacheL1, cacheL2, acc);
    } else if (st == Set_Prio_) {
        auto cacheL1 = std::unique_ptr<Cache<Set_Prioritized>>(new Cache<Set_Prioritized> (sizeL1, waysL1));
        auto cacheL2 = std::unique_ptr<Cache<Set_Prioritized>>(new Cache<Set_Prioritized> (sizeL2, waysL2));
        return run_cache (seed, cacheL1, cacheL2, acc);
    }
    return EXIT_FAILURE;
}

} // end of namespace dyad_Residency


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

    typedef std::vector<std::string> access;
    access acc; // access pattern in terms of the block index
    acc.push_back ("1");
    acc.push_back ("4");
    acc.push_back ("2");
    acc.push_back ("3");
    acc.push_back ("5");
    acc.push_back ("5");
    acc.push_back ("3");

    return cache_demo (set_type, seed, 4, 2, 8, 2, acc);
}
