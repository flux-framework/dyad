#include "generation.h" /* Defines the following:
                         *  - parse_cmd_line
                         *  - generate_vals
                         *  - NUM_VALS
                         */
#include <string.h>

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
int produce_file(char* full_path, int32_t seed, int32_t* val_buf)
{
    // Open the file or abort if not possible
    FILE* fp = fopen(full_path, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Cannot open file: %s\n", full_path);
        printf("File %ld: Cannot open\n", (long) seed);
        return -1;
    }
    // Write the file or abort if not possible
    size_t items_read = fwrite(val_buf, sizeof(int32_t), NUM_VALS, fp);
    // Close the file
    fclose(fp);
    // If an error occured during writing, abort
    if (items_read != NUM_VALS)
    {
        fprintf(stderr, "Could not write the full file (%s)\n", full_path);
        printf("File %ld: Cannot write\n", (long) seed);
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
    int num_chars;
    for (int32_t seed = 0; seed < num_transfers; seed++)
    {
        // Clear full_path to be safe
        memset(full_path, '\0', path_len+1);
        // Generate the file's data
        int32_t* val_buf = generate_vals(seed);
        // Generate the file name
        num_chars = snprintf(full_path, path_len+1, "%s/data%ld.txt", fpath, (long) seed);
        // If the file name could not be generated, print an error
        // and abort
        if (num_chars < 0)
        {
            fprintf(stderr, "Could not generate file name for transfer %ld\n", (long) seed);
            printf("File %ld: Bad Name\n", (long) seed);
            free(val_buf);
            continue;
        }
        /********************************************
         *     Perform DYAD Data Production!!!      *
         ********************************************/
        rc = produce_file(full_path, seed, val_buf);
        // Free the data that was saved to file
        free(val_buf);
        // If an error occured, continue
        if (rc != 0)
        {
            continue;
        }
    }
    free(full_path);
}
