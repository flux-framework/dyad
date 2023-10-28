from pydyad import dyad_open

import argparse
from pathlib import Path

import numpy as np


PROD_DIR = None


def produce_data(i, num_ints):
    global PROD_DIR
    fname = "file{}.npy".format(i)
    fname = PROD_DIR / fname
    start_val = i * num_ints
    int_buf = np.arange(start_val, start_val+num_ints, dtype=np.uint64)
    with dyad_open(fname, "wb") as f:
        np.save(f, int_buf)
    print("Successfully produced data (iter {})".format(i), flush=True)


def main():
    parser = argparse.ArgumentParser("Generates data for pydyad test")
    parser.add_argument("prod_managed_dir", type=Path,
                        help="DYAD's producer managed path")
    parser.add_argument("num_files", type=int,
                        help="Number of files to produce")
    parser.add_argument("num_ints", type=int,
                        help="Number of ints in each produced file")
    args = parser.parse_args()
    global PROD_DIR
    PROD_DIR = args.prod_managed_dir.expanduser().resolve()
    for i in range(args.num_files):
        print("Trying to produce data (iter {})".format(i), flush=True)
        produce_data(i, args.num_ints)


if __name__ == "__main__":
    main()
