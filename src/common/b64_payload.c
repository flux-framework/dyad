/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#if defined(__cplusplus)
#include <cstdlib>
#include <cerrno>
#include <cstring>
#else
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#endif // defined(__cplusplus)

#include <unistd.h>
#include <fcntl.h>

#include "b64_payload.h"

#include <b64/cencode.h>
#include <b64/cdecode.h>


/**
 * @brief Decode the data buffer encoded in B64 and write it out into a file.
 * @detail Turn the encoded data into the original binary form and write out
 *         to a file.
 *
 * @param[in] fd The descriptor of an output file
 * @param[in] buf_enc The pointer of the buffer containing encoded data
 * @param[in] len_enc The length of the data buffer
 * @param[in] chunk_size The size of an internal buffer for decoding
 *            If it is 0, same as \p len_enc
 *
 * @return    The size of decoded data (written into the file)
 */
ssize_t b64_decode_and_write (int fd, const char *buf_enc,
                              const size_t len_enc, size_t chunk_size)
{
    ssize_t n_enc = 0;    // size of a chunk encoded
    ssize_t n_dec = 0;    // size of a chunk decoded
    ssize_t n_wrt = 0;    // size of a chunk written
    ssize_t cnt_enc = 0;  // total amount read from encoded data buffer
    ssize_t cnt_dec = 0;  // total amount of data after decoding
    const char* buf_pos_enc = NULL; // starting position data to be decoded
    base64_decodestate ds;
    int saved_errno = errno;

    size_t chunk_sz = ((len_enc < chunk_size)?
                           len_enc :
                           (chunk_size == 0ul? len_enc : chunk_size));

    /* set up a buffer large enough to hold a chunk of decoded data to write
       ~3/4x of the encoded  */
    char *chunk_dec = (char*) malloc (chunk_sz);
    errno = 0;

    if (len_enc == 0) return 0;

    if ((fd < 0) || (buf_enc == NULL) |
        (chunk_sz == 0ul) || (chunk_dec == NULL)) {
        free (chunk_dec);
        errno = EINVAL;
        return -1;
    }

	/* initialise the decoder state */
	base64_init_decodestate (&ds);

    n_enc = (chunk_sz > len_enc)? len_enc : chunk_sz;

    do {
        //buf_pos_enc = (char*) buf_enc + cnt_enc;
        buf_pos_enc = buf_enc + cnt_enc;
        n_dec = base64_decode_block (buf_pos_enc, n_enc, chunk_dec, &ds);
        cnt_dec += n_dec;
        cnt_enc += n_enc;
        if ((n_wrt = write (fd, (void*) chunk_dec, n_dec)) < 0)
            goto error;
        n_enc = (cnt_enc + chunk_sz > len_enc)? (len_enc - cnt_enc) : chunk_sz;
    } while (n_enc != 0);

ok:
    free (chunk_dec);
    return cnt_dec;

error:
    free (chunk_dec);
    errno = saved_errno;
    return -1;
}


/**
 * @brief Encode the data read from a file by B64 and store them into a buffer.
 * @detail Encode the content of a binary data file into a text, which can be
 *         handled as ascii text. This makes it possible to embed binary data
 *         into a text-based JSON block.
 *
 * @param[in] fd The descriptor of an input file
 * @param[out] bufp The pointer of the output buffer containing encoded data
 * @param[in] chunk_size The size of an internal buffer for reading and encoding
 *            If it is 0, the default (65536 bytes) is used
 *
 * @return    The size of the encoded data buffer
 */
ssize_t read_and_b64_encode (int fd, char **bufp, size_t chunk_size)
{
    size_t len_enc = 0;   // total length of buffer allocated for encoded data
    void *buf_enc = NULL; // buffer pointer of data encoded
    ssize_t n = 0;        // size of a chuck read
    ssize_t n_enc = 0;    // size of a chunk encoded
    ssize_t cnt_enc = 0;  // total size of data encoded
    void *new_buf_enc = NULL; // temporary pointer for realloc
    char* buf_pos_enc = NULL; // starting position of empty buffer space
	base64_encodestate es;
    int saved_errno = errno;
    size_t chunk_sz = (chunk_size == 0ul? 65536ul : chunk_size);
    size_t chunk_sz_enc =  chunk_sz * 2u;
    /* set up a buffer large enough to hold a chunk of read data */
    char *chunk = (char*) malloc (chunk_sz * sizeof (char));
    /* set up a buffer large enough to hold a chunk of encoded data.
       ~4/3x of original */
    char *chunk_enc = (char*) malloc (chunk_sz_enc + 1u);
    errno = 0;

    if ((fd < 0 || !bufp) || (chunk == NULL) || (chunk_enc == NULL)) {
        free (chunk);
        free (chunk_enc);
        errno = EINVAL;
        return -1;
    }

	/* initialise the encoder state */
	base64_init_encodestate (&es);

    if ((n = read (fd, (void*) chunk, chunk_sz)) < 0)
        goto error;

    do {
        len_enc += chunk_sz_enc;
        if (!(new_buf_enc = realloc (buf_enc, len_enc)))
            goto error;
        buf_enc = new_buf_enc;
        buf_pos_enc = (char*) buf_enc + cnt_enc;

        n_enc = base64_encode_block ((void*) chunk, n, buf_pos_enc, &es);
        cnt_enc += n_enc;

        if ((n = read (fd, (void*) chunk, chunk_sz)) < 0)
            goto error;
    } while (n != 0);

	/* since we have reached the end of the input file, we know that
	   there is no more input data; finalise the encoding */
	n_enc = base64_encode_blockend (chunk_enc, &es);

    if (len_enc < cnt_enc + n_enc + 1) {
        len_enc = cnt_enc + n_enc;
        if (!(new_buf_enc = realloc (buf_enc, len_enc+1)))
            goto error;
        buf_enc = new_buf_enc;
    }
    buf_pos_enc = (char*) buf_enc + cnt_enc;
    memcpy (buf_pos_enc, chunk_enc, n_enc);

    cnt_enc += n_enc;
    ((char*) buf_enc) [cnt_enc] = '\0';
    *bufp = (char*) buf_enc;

ok:
    free (chunk);
    free (chunk_enc);
    return cnt_enc;

error:
    free (chunk);
    free (chunk_enc);
    errno = saved_errno;
    return -1;
}


#if 0 // keeping the test here temporarily
#include <stdio.h>
#include <linux/limits.h> // PATH_MAX
#include <time.h>
double diff_time (struct timespec t_start, struct timespec t_end)
{
    struct timespec t_diff;

    t_diff.tv_nsec = t_end.tv_nsec - t_start.tv_nsec;
    t_diff.tv_sec  = t_end.tv_sec - t_start.tv_sec;

    if (t_start.tv_sec > t_end.tv_sec)
        return -1;

    if ((t_start.tv_sec == t_end.tv_sec) &&
        (t_start.tv_nsec > t_end.tv_nsec))
        return -1;

    if (t_diff.tv_sec > 0 && t_diff.tv_nsec < 0)
    {
        t_diff.tv_nsec += 1000000000;
        t_diff.tv_sec--;
    }
    else if (t_diff.tv_sec < 0 && t_diff.tv_nsec > 0)
    {
        t_diff.tv_nsec -= 1000000000;
        t_diff.tv_sec++;
    }
    return t_diff.tv_sec + t_diff.tv_nsec / 1000000000.0;
}

int main (int argc, char** argv)
{
    int in_file = -1;
    int enc_file = -1;
    int out_file = -1;
    char* buf_enc = NULL;
    ssize_t buf_sz = 0;
    ssize_t file_sz = 0;
    char enc_fname [PATH_MAX+1] = {'\0'};
    size_t fname_len = 0u;
    struct timespec t1, t2, t3;

	if (argc < 3) {
        printf ("Usage: %s in_file out_file write_enc\n", argv[0]);
        exit (-1);
	}

    in_file  = open (argv[1], O_RDONLY);
    strncpy (enc_fname, argv[1], PATH_MAX);
    fname_len = strlen (argv[1]);
    strncpy (enc_fname + fname_len, "-b64_enc.txt", PATH_MAX - fname_len);
    out_file = open (argv[2], O_WRONLY | O_CREAT | O_SYNC, 0666);

    clock_gettime (CLOCK_MONOTONIC_RAW, &t1);
    buf_sz = read_and_b64_encode (in_file, &buf_enc, 0);
    close (in_file);
    clock_gettime (CLOCK_MONOTONIC_RAW, &t2);

    if (buf_enc == NULL) {
        printf ("Encoding failed\n");
        return EXIT_FAILURE;
    }

    if ((argc >= 4) && (atoi (argv[3]))) {
        enc_file = open (enc_fname, O_WRONLY | O_CREAT | O_SYNC, 0666);
        if (enc_file < 0) {
            free (buf_enc);
            return EXIT_FAILURE;
        }
        write (enc_file, (const void*) buf_enc, buf_sz);
        close (enc_file);
        printf ("Encoded file: %s\n", enc_fname);
    }

    file_sz = b64_decode_and_write (out_file, buf_enc, buf_sz, 0);
    close (out_file);
    free (buf_enc);
    clock_gettime (CLOCK_MONOTONIC_RAW, &t3);

    printf ("sizes of encoded and decoded data: %ld %ld\n", buf_sz, file_sz);
    printf ("Tenc = %f \tTdec = %f\n", diff_time (t1, t2), diff_time (t2, t3));

	return EXIT_SUCCESS;
}
#endif

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
