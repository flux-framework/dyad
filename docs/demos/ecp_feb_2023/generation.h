#ifndef __GENERATION_H__
#define __GENERATION_H__

#ifdef __cplusplus
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C"
{
#else
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#endif // __cplusplus

#include <sys/stat.h>

// Number of values per file
#define NUM_VALS 10

// Simple macros to get the size of the value buffer
// (in bytes) and to perform allocation of the value buffer
#define VAL_BUF_SIZE NUM_VALS*sizeof(int32_t)
#define ALLOC_VAL_BUF() (int32_t*)malloc(VAL_BUF_SIZE)

/**
 * Generate a deterministic set of values given a seed value
 *
 * @param[in] seed The seed value used to create the set of values
 *
 * @return An array of int32_t's generated from seed
 */
int32_t* generate_vals(int32_t seed)
{
    // Determine what value to start with in the array
    int32_t start_val = seed * NUM_VALS;
    // Allocate a buffer for the array
    int32_t* val_buf = ALLOC_VAL_BUF();
    // Create an array containing NUM_VALS integers starting with start_val
    for (int32_t v = start_val; v < start_val + NUM_VALS; v++)
    {
        val_buf[v - start_val] = v;
    }
    return val_buf;
}

/**
 * Verify a set of values that should have been originally
 * produced by generate_vals
 *
 * @param[in] seed    The seed value associated with the set of values
 * @param[in] val_buf The set of values to validate
 *
 * @return true if the contents of val_buf are correct for the given value of seed
 */
bool vals_are_valid(int32_t seed, int32_t* val_buf)
{
    // Determine what value should start the array
    int32_t start_val = seed * NUM_VALS;
    bool is_valid = true;
    // For each element in the array, confirm that the value is
    // what is expected
    for (int32_t v = start_val; v < start_val + NUM_VALS; v++)
    {
        is_valid = is_valid & (val_buf[v - start_val] == v);
    }
    return is_valid;
}

/**
 * Parse the command-line arguments for the consumer
 *
 * @param[in] argc           Number of command line arguments
 * @param[in] argv           Array of command-line arguments
 * @param[out] num_transfers Number of files to consume
 * @param[out] fpath         Path to the consumer's destination directory
 *
 * @return 0 if no errors occured. -1 otherwise.
 */
int parse_cmd_line(int argc, char** argv, int32_t* num_transfers, char** fpath)
{
    // Print an error and abort if an invalid number of arguments
    // were provided
    if (argc != 3)
    {
        fprintf(stderr, "Usage: ./c_cons <# of Files Transferred> <Consumer Directory>\n");
        return -1;
    }
    // Ensure a valid number of file transfers were provided
    *num_transfers = (int32_t) atol(argv[1]);
    if (*num_transfers <= 0)
    {
        fprintf(stderr, "Either an invalid number of transfers was provided, \
                or an error occured in parsing the argument!\n");
       return -1;
    }
    // Get the Consumer Directory from command line and
    // make sure the directory exists
    *fpath = argv[2];
    struct stat finfo;
    if (stat(*fpath, &finfo) != 0 || !S_ISDIR(finfo.st_mode))
    {
        fprintf(stderr, "The provided directory (%s) does not exist!\n", *fpath);
        return -1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* __GENERATION_H__ */
