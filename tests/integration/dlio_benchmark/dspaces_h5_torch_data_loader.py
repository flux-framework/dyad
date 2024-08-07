"""
   Copyright (c) 2022, UChicago Argonne, LLC
   All Rights Reserved

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
"""
from time import time
import logging
import math
import pickle
import torch
from torch.utils.data import Dataset, DataLoader, RandomSampler, SequentialSampler

from dlio_benchmark.common.constants import MODULE_DATA_LOADER
from dlio_benchmark.common.enumerations import Shuffle, DatasetType, DataLoaderType
from dlio_benchmark.data_loader.base_data_loader import BaseDataLoader
from dlio_benchmark.reader.reader_factory import ReaderFactory
from dlio_benchmark.utils.utility import utcnow, DLIOMPI
from dlio_benchmark.utils.config import ConfigArguments
from dlio_profiler.logger import fn_interceptor as Profile

import dspaces
import numpy as np
import os
import h5py
import fcntl
dlp = Profile(MODULE_DATA_LOADER)


class DspacesH5TorchDataset(Dataset):
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
        args = ConfigArguments.get_instance()
        self.img_dim = args.dimension
        self.serial_args = pickle.dumps(args)
        self.dlp_logger = None
        if num_workers == 0:
            self.worker_init(-1)

    @dlp.log
    def worker_init(self, worker_id):
        pickle.loads(self.serial_args)
        self._args = ConfigArguments.get_instance()
        self._args.configure_dlio_logging(is_child=True)
        self.dlp_logger = self._args.configure_dlio_profiler(is_child=True, use_pid=True)
        logging.debug(f"{utcnow()} worker initialized {worker_id} with format {self.format_type}")
        self.reader = ReaderFactory.get_reader(type=self.format_type,
                                               dataset_type=self.dataset_type,
                                               thread_index=worker_id,
                                               epoch_number=self.epoch_number)
        proc_rank = os.getpid()
        logging.debug("Intializing dspaces")
        self.ds_client = dspaces.dspaces(rank=proc_rank)

    def __del__(self):
        if self.dlp_logger:
            self.dlp_logger.finalize()
        # Manually invoke finalizer for DataSpaces to ensure it is shutdown properly
        # if self.ds_client:
        #     del self.ds_client
        #     self.ds_client = None
            
    @dlp.log
    def __len__(self):
        return self.num_samples

    @dlp.log
    def __getitem__(self, image_idx):
        logging.debug(f"{utcnow()} Rank {DLIOMPI.get_instance().rank()} reading {image_idx} image")
        self.num_images_read += 1
        step = int(math.ceil(self.num_images_read / self.batch_size))
        filename, sample_index = self._args.global_index_map[image_idx]
        lb = tuple([sample_index, 0, 0])
        ub = tuple([sample_index, self.img_dim-1, self.img_dim-1])
        dlp.update(args={"fname":filename})
        logging.debug(f"Filename is {filename}")
        dlp.update(args={"image_idx":image_idx})
        dlp.update(args={"version":0})
        dlp.update(args={"lb":lb})
        logging.debug(f"lb is {lb}")
        dlp.update(args={"ub":ub})
        logging.debug(f"ub is {ub}")
        logging.debug("Starting dspaces aget")
        data = self.ds_client.get(
            filename,  # variable name
            0,         # variable version
            lb,        # lower bound in global dims
            ub,        # upper bound in global dims
            np.uint8,  # NumPy datatype of elements
            -1         # timeout
        )
        logging.debug("Finished dspaces aget")
        dlp.update(step=step)
        logging.debug(f"data shape is {data.shape}")
        dlp.update(image_size=data.nbytes)
        return data

class DspacesH5TorchDataLoader(BaseDataLoader):
    @dlp.log_init
    def __init__(self, format_type, dataset_type, epoch_number):
        super().__init__(format_type, dataset_type, epoch_number, DataLoaderType.PYTORCH)

    @dlp.log
    def read(self):
        do_shuffle = True if self._args.sample_shuffle != Shuffle.OFF else False
        dataset = DspacesH5TorchDataset(self.format_type, self.dataset_type, self.epoch_number, self.num_samples, self._args.read_threads, self.batch_size)
        if do_shuffle:
            sampler = RandomSampler(dataset)
        else:
            sampler = SequentialSampler(dataset)
        if self._args.read_threads >= 1:
            prefetch_factor = math.ceil(self._args.prefetch_size / self._args.read_threads)
        else:
            prefetch_factor = self._args.prefetch_size
        if prefetch_factor > 0:
            if self._args.my_rank == 0:
                logging.debug(
                    f"{utcnow()} Prefetch size is {self._args.prefetch_size}; prefetch factor of {prefetch_factor} will be set to Torch DataLoader.")
        else:
            prefetch_factor = 2
            if self._args.my_rank == 0:
                logging.debug(
                    f"{utcnow()} Prefetch size is 0; a default prefetch factor of 2 will be set to Torch DataLoader.")
        logging.debug(f"{utcnow()} Setup dataloader with {self._args.read_threads} workers {torch.__version__}")
        if self._args.read_threads==0:
            kwargs={}
        else:
            kwargs={'multiprocessing_context':self._args.multiprocessing_context,
                    'prefetch_factor': prefetch_factor}
            if torch.__version__ != '1.3.1':
                kwargs['persistent_workers'] = True
        if torch.__version__ == '1.3.1':
            if 'prefetch_factor' in kwargs:
                del kwargs['prefetch_factor']
            self._dataset = DataLoader(dataset,
                                       batch_size=self.batch_size,
                                       sampler=sampler,
                                       num_workers=self._args.read_threads,
                                       pin_memory=True,
                                       drop_last=True,
                                       worker_init_fn=dataset.worker_init,
                                       **kwargs)
        else:
            self._dataset = DataLoader(dataset,
                                       batch_size=self.batch_size,
                                       sampler=sampler,
                                       num_workers=self._args.read_threads,
                                       pin_memory=True,
                                       drop_last=True,
                                       worker_init_fn=dataset.worker_init,
                                       **kwargs)  # 2 is the default value
        logging.debug(f"{utcnow()} Rank {self._args.my_rank} will read {len(self._dataset) * self.batch_size} files")

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
