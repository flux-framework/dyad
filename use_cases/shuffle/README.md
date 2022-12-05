Use case demonstration for input file shuffling with deep learning. 

Deep learning training often requires randomizing the order of input samples at
each epoch. In distributed or parallel training where each worker consumes a
subset of samples, the set of samples to read changes for each worker at every
epoch due to the randomization. 
In this demo, we assume that each sample is read from a unique file such as an
image file. We also assume that the amount of input samples are is large to fit
in a single local storage of a worker. Therefore, input data resides on a shared
storage. 
With DYAD, a worker will be able to avoid loading files from a shared storage at
every epoch. Instead, it can pull them from the local storages of other workers. 
At the beginning, the entire set will be partitioned across workers. Each worker
loads files from its partition into its local storage. 
We currently mimic this by creating a set of files under a DYAD-managed
directory on the local storage. Each worker can access any sample as if it exists
locally. 
As the capacity of a local storage reaches its limit, some files will have to be
randomly evicted, especially when loading a new file.
