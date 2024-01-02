from time import time
import logging
import math
import torch
from torch.utils.data import Dataset, DataLoader, RandomSampler, SequentialSampler

from dlio_benchmark.common.constants import MODULE_DATA_LOADER
from dlio_benchmark.common.enumerations import Shuffle, DatasetType, DataLoaderType
from dlio_benchmark.data_loader.base_data_loader import BaseDataLoader
from dlio_benchmark.reader.reader_factory import ReaderFactory
from dlio_benchmark.utils.utility import utcnow, get_rank, Profile
from dlio_benchmark.utils.config import ConfigArguments
from pydyad import Dyad, dyad_open
from pydyad.bindings import DTLMode
import numpy as np
dlp = Profile(MODULE_DATA_LOADER)
import os

class DYADTorchDataset(Dataset):
    """
    Currently, we only support loading one sample per file
    TODO: support multiple samples per file
    """
    @dlp.log_init
    def __init__(self, format_type, dataset_type, epoch, num_samples, num_workers, batch_size):
        self.format_type = format_type
        self.dataset_type = dataset_type
        self.epoch_number = epoch
        self.num_samples = num_samples
        self.reader = None
        self.num_images_read = 0
        self.batch_size = batch_size
        if num_workers == 0:
            self.worker_init(-1)

    @dlp.log
    def worker_init(self, worker_id):
        logging.debug(f"{utcnow()} worker initialized {worker_id} with format {self.format_type}")
        self.reader = ReaderFactory.get_reader(type=self.format_type,
                                               dataset_type=self.dataset_type,
                                               thread_index=worker_id,
                                               epoch_number=self.epoch_number)
        self.conf = ConfigArguments.get_instance()
        self.dyad_io = Dyad()
        is_local = os.getenv("DYAD_LOCAL_TEST", "0") == "1"
        import flux
        self.f = flux.Flux()
        if is_local:
            self.dyad_managed_directory = os.path.join(os.getenv("DYAD_PATH", ""), str(self.f.get_rank()))
        else:
            self.dyad_managed_directory = os.getenv("DYAD_PATH", "")
        dtl_str = os.getenv("DYAD_DTL_MODE", "FLUX_RPC")
        mode = DTLMode.DYAD_DTL_FLUX_RPC
        if dtl_str == "UCX":
            mode = DTLMode.DYAD_DTL_UCX
        self.dyad_io.init(debug=self.conf.debug, check=False, shared_storage=False, key_depth=3,
                          key_bins=1024, kvs_namespace=os.getenv("DYAD_KVS_NAMESPACE"),
                          prod_managed_path=self.dyad_managed_directory, cons_managed_path=self.dyad_managed_directory,
                          dtl_mode=mode)

    @dlp.log
    def __len__(self):
        return self.num_samples

    @dlp.log
    def __getitem__(self, image_idx):
        self.num_images_read += 1
        step = int(math.ceil(self.num_images_read / self.batch_size))
        logging.info(f"{utcnow()} Rank {get_rank()} reading {image_idx} sample")
        filename, sample_index = self.conf.global_index_map[image_idx]
        is_present = False
        file_obj = None
        base_fname = filename
        if self.dyad_managed_directory != "":
            logging.debug(f"Using managed directory {self.dyad_managed_directory}")
            base_fname = os.path.join(self.dyad_managed_directory, os.path.basename(filename))
            file_obj = self.dyad_io.get_metadata(fname=base_fname, should_wait=False)
            is_present = True
        if file_obj:
            logging.debug(f"Reading from managed directory {base_fname}")
            with dyad_open(base_fname, "rb", dyad_ctx=self.dyad_io) as f:
                data = np.load(f, allow_pickle=True)["x"]
        else:
            logging.debug(f"Reading from pfs {base_fname}")
            data = self.reader.read_index(image_idx, step)
            if is_present:
                logging.debug(f"Writing to managed_directory {base_fname}")
                with dyad_open(base_fname, "wb", dyad_ctx=self.dyad_io) as f:
                    np.savez(f, x=data)
        return data

class DyadTorchDataLoader(BaseDataLoader):
    @dlp.log_init
    def __init__(self, format_type, dataset_type, epoch_number):
        super().__init__(format_type, dataset_type, epoch_number, DataLoaderType.PYTORCH)

    @dlp.log
    def read(self):
        do_shuffle = True if self._args.sample_shuffle != Shuffle.OFF else False
        num_samples = self._args.total_samples_train if self.dataset_type is DatasetType.TRAIN else self._args.total_samples_eval
        batch_size = self._args.batch_size if self.dataset_type is DatasetType.TRAIN else self._args.batch_size_eval
        dataset = DYADTorchDataset(self.format_type, self.dataset_type, self.epoch_number, num_samples, self._args.read_threads, batch_size)
        if do_shuffle:
            sampler = RandomSampler(dataset)
        else:
            sampler = SequentialSampler(dataset)
        if self._args.read_threads > 1:
            prefetch_factor = math.ceil(self._args.prefetch_size / self._args.read_threads)
        else:
            prefetch_factor = self._args.prefetch_size
        if prefetch_factor > 0:
            if self._args.my_rank == 0:
                logging.debug(
                    f"{utcnow()} Prefetch size is {self._args.prefetch_size}; prefetch factor of {prefetch_factor} will be set to Torch DataLoader.")
        else:
            if self._args.my_rank == 0:
                logging.debug(
                    f"{utcnow()} Prefetch size is 0; a default prefetch factor of 2 will be set to Torch DataLoader.")
        logging.debug(f"{utcnow()} Setup dataloader with {self._args.read_threads} workers {torch.__version__}")
        if torch.__version__ == '1.3.1':
            self._dataset = DataLoader(dataset,
                                       batch_size=batch_size,
                                       sampler=sampler,
                                       num_workers=self._args.read_threads,
                                       pin_memory=True,
                                       drop_last=True,
                                       worker_init_fn=dataset.worker_init)
        else:
            self._dataset = DataLoader(dataset,
                                       batch_size=batch_size,
                                       sampler=sampler,
                                       num_workers=self._args.read_threads,
                                       pin_memory=True,
                                       drop_last=True,
                                       worker_init_fn=dataset.worker_init,
                                       prefetch_factor=prefetch_factor if prefetch_factor > 0 else 2)  # 2 is the default value
        logging.debug(f"{utcnow()} Rank {self._args.my_rank} will read {len(self._dataset) * batch_size} files")

        # self._dataset.sampler.set_epoch(epoch_number)

    @dlp.log
    def next(self):
        super().next()
        total = self._args.training_steps if self.dataset_type is DatasetType.TRAIN else self._args.eval_steps
        logging.debug(f"{utcnow()} Rank {self._args.my_rank} should read {total} batches")
        for batch in self._dataset:
            yield batch

    @dlp.log
    def finalize(self):
        pass