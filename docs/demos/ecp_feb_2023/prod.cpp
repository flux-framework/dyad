#include "generation.h"

#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <string>

#include <dyad_stream_api.hpp>

/**
 * Write the spcified file and record it in for transfer with DYAD
 *
 * @param[in] full_path Path to the file to read
 * @param[in] seed      The "seed value" (i.e., number from the for-loop in main)
 *                      for the file
 * @param[in] val_buf   Array in which the contents of the file are stored
 *
 * @return 0 if no errors occured. -1 otherwise.
 */
int produce_file(std::string& full_path, int32_t seed, int32_t* val_buf)
{
    // Open the file or abort if not possible
    dyad::ofstream_dyad ofs_dyad;
    try
    {
        ofs_dyad.open(full_path, std::ofstream::out | std::ios::binary);
    }
    catch (std::ios_base::failure)
    {
        std::cerr << "Cannot open file: " << full_path << "\n";
        std::cout << "File " << seed << ": Cannot open\n";
        return -1;
    }
    // Get the C++ std::ofstream from the DYAD stream
    std::ofstream& ofs = ofs_dyad.get_stream();
    // Write the file or abort if not possible
    try
    {
        ofs.write((char*) val_buf, VAL_BUF_SIZE);
    }
    catch (std::ios_base::failure)
    {
        ofs_dyad.close();
        std::cerr << "Could not write the full file (" << full_path << ")\n";
        std::cout << "File " << seed << ": Cannot write\n";
        return -1;
    }
    // If everything went well, close the file
    ofs_dyad.close();
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
    int num_chars;
    size_t items_read;
    for (int32_t seed = 0; seed < num_transfers; seed++)
    {
        // Generate the file's data
        int32_t* val_buf = generate_vals(seed);
        // Generate the file name
        full_path = cpp_fpath + "/data" + std::to_string(seed) + ".txt";
        /********************************************
         *     Perform DYAD Data Production!!!      *
         ********************************************/
        rc = produce_file(full_path, seed, val_buf);
        free(val_buf);
        if (rc != 0)
        {
            continue;
        }
    }
}
