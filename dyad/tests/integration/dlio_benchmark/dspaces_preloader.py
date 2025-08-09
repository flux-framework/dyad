from mpi4py import MPI
from pathlib import Path
import h5py
import dspaces

import argparse
import logging
import time

VALID_HDF5_EXTS = [".hdf5", ".h5"]


def collect_file_names(dirname):
    return list(sorted([f for f in dirname.iterdir() if f.is_file() and f.suffix in VALID_HDF5_EXTS]))


def get_local_files(all_files, rank, comm_size):
    return [all_files[i] for i in range(rank, len(all_files), comm_size)]


def store_samples_for_file(ds_client, local_file):
    with h5py.File(str(local_file), "r") as f:
        for starting_sample in range(0, f["records"].shape[0], 256):
            end = starting_sample+256
            if end > f["records"].shape[0]:
                end = f["records"].shape[0]
            data = f["records"][starting_sample:end]
            offset = (starting_sample, 0, 0)
            ds_client.put(data, str(local_file), 0, offset)

        
def main():
    parser = argparse.ArgumentParser("Preload data for MuMMI training")
    parser.add_argument("data_dir", type=Path,
                        help="Path to the directory containing HDF5 files")
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    loglevel = logging.WARNING
    if args.verbose:
        loglevel = logging.INFO
    elif args.debug:
        loglevel = logging.DEBUG
    logging.basicConfig(level=loglevel,
        handlers=[
            logging.StreamHandler()
        ],
        format='[%(levelname)s] [%(asctime)s] %(message)s [%(pathname)s:%(lineno)d]',
        datefmt='%H:%M:%S'
    )
    data_dir = args.data_dir.expanduser().resolve()
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    comm_size = comm.Get_size()
    start = time.time()
    if rank == 0:
        logging.info("Preloading with {} processes".format(comm_size))
    logging.debug("RANK {}: initializing DataSpaces".format(rank))
    ds_client = dspaces.dspaces(comm=comm)
    logging.debug("RANK {}: collecting filenames".format(rank))
    all_files = collect_file_names(data_dir)
    logging.debug("RANK {}: obtaining local files".format(rank))
    local_files = get_local_files(all_files, rank, comm_size)
    logging.debug("RANK {}: putting the following files: {}".format(rank, local_files))
    for lf in local_files:
        store_samples_for_file(ds_client, lf)
    end = time.time()
    local_time = end - start
    total_time = 0.0
    total_time = comm.reduce(local_time,MPI.SUM, root=0)
    if rank == 0:
        total_time /= comm_size
        print("DataSpaces preload time is {} s".format(total_time), flush=True)
    comm.Barrier()
    comm.Abort(0) # Perform an abort because finalization sometimes hangs on Corona
    

if __name__ == "__main__":
    main()
