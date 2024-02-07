#include <dyad/residency/fcache.hpp>
#include <string>
#include <iostream>
#include <cstdlib>

int main (int argc, char** argv)
{
    using namespace dyad_residency;

    int seed = 0;

    if (argc > 2) {
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        seed = atoi (argv[1]);
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
    bool hitL1;

#if 1
    Cache<Set_LRU> cacheL1 (4, 2); // 2-way set-associative
    Cache<Set_LRU> cacheL2 (8, 2); // cache capacity = 8 blocks
#else
    Cache<Set_LRU> cacheL1 (4, 0); // cache capacity = 4 blocks
    Cache<Set_LRU> cacheL2 (8, 0); // fully-associative
#endif
    cacheL1.set_seed (seed + 104677u);
    cacheL1.set_level ("L1");
    cacheL2.set_seed (seed + 104681u);
    cacheL2.set_level ("  L2");

    std::cout << "L1 Cache size set to " << cacheL1.size () << " blocks" << std::endl;
    std::cout << "L2 Cache size set to " << cacheL2.size () << " blocks" << std::endl;

    std::cout << "accessing block 1, 4, 2, and 3 in order" << std::endl;
    for (unsigned int i = 0; (i < cacheL1.size ()) && (i < acc.size ()); i++) {
        hitL1 = cacheL1.access (acc[i]);
        if (!hitL1) cacheL2.access (acc[i]);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing block 5" << std::endl;
    hitL1 = cacheL1.access (acc[4]);
    if (!hitL1) cacheL2.access (acc[4]);

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    std::cout << "accessing block 1, 4, 2, 3, 5, 5, and 3 in order" << std::endl;
    for (unsigned int i = 0; i < acc.size (); i++) {
        hitL1 = cacheL1.access (acc[i]);
        if (!hitL1) cacheL2.access (acc[i]);
    }

    std::cout << "-------------------------L1----------------------------" << std::endl;
    std::cout << cacheL1 << std::endl;
    std::cout << "-------------------------L2----------------------------" << std::endl;
    std::cout << cacheL2 << std::endl;
    std::cout << "-------------------------------------------------------" << std::endl;

    return EXIT_SUCCESS;
}
