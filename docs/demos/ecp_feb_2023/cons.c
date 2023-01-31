#include "generation.h" /* Defines the following:
                         *  - parse_cmd_line
                         *  - ALLOC_VAL_BUF
                         *  - vals_are_valid
                         *  - VAL_BUF_SIZE
                         *  - NUM_VALS
                         */
#include <string.h>

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
int consume_file(char* full_path, int32_t seed, int32_t* val_buf)
{
    // Open the file or abort if not possible
    FILE* fp = fopen(full_path, "r");
    // Print an error if the file couldn't be opened
    if (fp == NULL)
    {
        fprintf(stderr, "Cannot open file: %s\n", full_path);
        printf("File %ld: Cannot open\n", (long) seed);
        return -1;
    }
    // Read the file or abort if not possible
    size_t items_read = fread(val_buf, sizeof(int32_t), NUM_VALS, fp);
    fclose(fp);
    // Print an error if the number of items read is not what was expected
    if (items_read != NUM_VALS)
    {
        fprintf(stderr, "Could not read the full file (%s)\n", full_path);
        printf("File %ld: Cannot read\n", (long) seed);
        return -1;
    }
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
    // Largest number of digits for a int32_t when converted to string
    const size_t max_digits = 10;
    // First 4 is for "data"
    // Second 4 is for ".txt"
    size_t path_len = strlen(fpath) + 4 + max_digits + 4;
    // Allocate a buffer for the file path
    char* full_path = malloc(path_len + 1);
    // Allocate a buffer for the data read from the file
    int32_t* val_buf = ALLOC_VAL_BUF();
    int num_chars;
    for (int32_t seed = 0; seed < num_transfers; seed++)
    {
        // Clear full_path and val_buf to be safe
        memset(full_path, '\0', path_len+1);
        memset(val_buf, 0, VAL_BUF_SIZE);
        // Generate the file name
        num_chars = snprintf(full_path, path_len+1, "%s/data%ld.txt", fpath, (long) seed);
        // If the file name could not be generated, print an error
        // and abort
        if (num_chars < 0)
        {
            fprintf(stderr, "Could not generate file name for transfer %ld\n", (long) seed);
            printf("File %ld: Bad Name\n", (long) seed);
            continue;
        }
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
            printf("File %ld: OK\n", (long) seed);
        }
        else
        {
            printf("File %ld: BAD\n", (long) seed);
        }
    }
    free(val_buf);
    free(full_path);
}
