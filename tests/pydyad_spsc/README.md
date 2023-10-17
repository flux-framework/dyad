# PyDYAD Single Producer-Single Consumer Test

This directory contains a very simple test pipeline to ensure PyDYAD is working. It consists of two tasks: a producer and a consumer. Both tasks run for a fixed number of iterations. In each iteration, the producer creates a NumPy array consisting of consecutive 64-bit integers starting from the product of the iteration ID and number of integers per iteration. Similarly, in each iteration, the consumer creates a NumPy array in the same way as the producer. Then, the consumer reads the file created by the producer and verifies that the file's contents.

## Usage

The usage of these tasks is below. The parameters for these tasks is as follows:
* `producer_managed_directory`: the directory where the producer will write data. Can also be considered as DYAD's producer managed directory
* `consumer_managed_directory`: the directory from which the consumer will read data. Can also be considered as DYAD's consumer managed directory
* `num_files_to_transfer`: the number of files that they producer will write and that the consumer will read. Can also be considered the number of iterations
* `num_ints_expected`: the number of randomly generated integers that will be stored in each file

__Producer Task:__
```bash
$ python3 producer.py <producer_managed_directory> <num_files_to_transfer> <num_ints_expected>
```

__Consumer Task:__
```bash
$ python3 consumer.py <consumer_managed_directory> <num_files_to_transfer> <num_ints_expected>
```

To run the test, you can either create your own script, or you can use `run.sh`. To use `run.sh`, first set the variables at the top of the file. Then, launch the job with `flux batch run.sh`.