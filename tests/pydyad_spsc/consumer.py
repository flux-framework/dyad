from pydyad import dyad_open

import argparse
from pathlib import Path

import numpy as np


CONS_DIR = None


def consume_data(i, num_ints_expected):
    global CONS_DIR
    fname = "file{}.npy".format(i)
    fname = CONS_DIR / fname
    start_val = i * num_ints_expected
    verify_int_buf = np.arange(
        start_val,
        start_val+num_ints_expected,
        dtype=np.uint64
    )
    int_buf = None
    with dyad_open(fname, "rb") as f:
        int_buf = np.load(f)
    if int_buf is None:
        raise RuntimeError("Could not read {}".format(fname))
    if int_buf.size != num_ints_expected:
        raise RuntimeError(
            "Consumed data has incorrect size {n_read} != {expected}".format(
             n_read = int_buf.size, expected = num_ints_expected)
        )
    if not np.array_equal(int_buf, verify_int_buf):
        raise RuntimeError("Consumed data is incorrect!")
    print("Correctly consumed data (iter {})".format(i), flush=True)


def main():
    parser = argparse.ArgumentParser("Consumes data for pydyad test")
    parser.add_argument("cons_managed_dir", type=Path,
                        help="DYAD's consumer managed path")
    parser.add_argument("num_files", type=int,
                        help="Number of files to consume")
    parser.add_argument("num_ints_expected", type=int,
                        help="Number of ints expected in each consumed file")
    args = parser.parse_args()
    global CONS_DIR
    CONS_DIR = args.cons_managed_dir.expanduser().resolve()
    for i in range(args.num_files):
        print("Trying to consume data (iter {})".format(i), flush=True)
        consume_data(i, args.num_ints_expected)


if __name__ == "__main__":
    main()
