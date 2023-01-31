#include "generation.h"

#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <string>

#include <dyad_stream_api.hpp>

/**
 * Transfer the specified file with DYAD and read it
 *
 * @param[in] full_path Path to the file to read
 * @param[in] seed      The "seed value" (i.e., number from the for-loop in main)
 *                      for the file
 * @param[out] val_buf  Array in which to store the contents of the file
 *
 * @return 0 if no errors occured. -1 otherwise.
 */
int consume_file(std::string& full_path, int32_t seed, int32_t* val_buf)
{
    // Open the file.
    // If not possible, print an error and abort
    dyad::ifstream_dyad ifs_dyad;
    try
    {
        ifs_dyad.open(full_path, std::ifstream::in | std::ios::binary);
    }
    catch (std::ios_base::failure)
    {
        std::cerr << "Could not open file: " << full_path << "\n";
        std::cout << "File " << seed << ": Cannot open\n";
        return -1;
    }
    // Get the C++ std::ifstream from the DYAD stream
    std::ifstream& ifs = ifs_dyad.get_stream();
    // Read in the contents of the file using the ifstream.
    // If the read fails, print an error and abort
    try
    {
        ifs.read((char*) val_buf, VAL_BUF_SIZE);
    }
    catch (std::ios_base::failure)
    {
        ifs_dyad.close();
        std::cerr << "Could not read the full file (" << full_path << ")\n";
        std::cout << "File " << seed << ": Cannot read\n";
        return -1;
    }
    // If everything went well, close the file
    ifs_dyad.close();
    return 0;
}

int main(int argc, char** argv)
{
    // Parse command-line arguments
    int32_t num_transfers;
    char* fpath;
    int rc = parse_cmd_line(argc, argv, &num_transfers, &fpath);
    // If an error occured during command-line parsing,
    // abort the consumer
    if (rc != 0)
    {
        return rc;
    }
    // Convert the managed directory path to a C++ string
    std::string cpp_fpath = fpath;
    std::string full_path;
    // Allocate a buffer for the data read from the file
    int32_t* val_buf = ALLOC_VAL_BUF();
    int num_chars;
    size_t items_read;
    for (int32_t seed = 0; seed < num_transfers; seed++)
    {
        // Clear val_buf to be safe
        memset(val_buf, 0, VAL_BUF_SIZE);
        // Generate the file name
        full_path = cpp_fpath + "/data" + std::to_string(seed) + ".txt";
        /********************************************
         *       Perform DYAD Data Transfer!!!      *
         ********************************************/
        rc = consume_file(full_path, seed, val_buf);
        if (rc != 0)
        {
            continue;
        }
        // Validate the content of the file
        if (vals_are_valid(seed, val_buf))
        {
            std::cout << "File " << seed << ": OK\n";
        }
        else
        {
            std::cout << "File " << seed << ": BAD\n";
        }
    }
    free(val_buf);
}
