from mpi4py import MPI
from pathlib import Path
import h5py
import dspaces

import argparse

VALID_HDF5_EXTS = [".hdf5", ".h5"]


def collect_file_names(dirname):
    return list(sorted([f for f in dirname.iterdir() if f.is_file() and f.suffix() in VALID_HDF5_EXTS]))


def get_local_files(all_files, rank, comm_size):
    return [all_files[i] for i in range(rank, len(all_files), comm_size)]


def store_samples_for_file(ds_client, local_file):
    with h5py.File(str(local_file), "r") as f:
        data = f["records"][:]
        offset = tuple([0] * len(data.shape))
        ds_client.put(data, str(local_file), 0, offset)

        
def main():
    parser = argparse.ArgumentParser("Preload data for MuMMI training")
    parser.add_argument("data_dir", type=Path,
                        help="Path to the directory containing HDF5 files")
    args = parser.parse_args()
    data_dir = args.data_dir.expanduser().resolve()
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    comm_size = comm.Get_size()
    ds_client = dspaces.dspaces(comm=comm)
    all_files = collect_file_names(data_dir)
    local_files = get_local_files(all_files, rank, comm_size)
    for lf in local_files:
        store_samples_for_file(ds_client, lf)
    

if __name__ == "__main__":
    main()