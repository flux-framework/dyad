# DYAD producer-consumer example with Merlin

In this simplest scenario, there is one task (producer) that writes a set of
files, and another task (consumer) that reads the files. With total 2 nodes,
the producer and the consumer can run on different nodes.

There are three workflow spec files for this scenario that achieve the same
in different ways. The first one below represents the usual method in Merlin.
The other two take advantage of the FLUX module, DYAD, and begin with the
`startup` step, in which DYAD module is loaded into FLUX.

# `dependent_step.yaml` 
+ The `producer` step is the first step. The `consumer` step depends on the
  producer step. The producer writes files on a shared file system, that is
  visible by both the producer and the consumer.
  Merlin makes sure that the consumer accesses the files after the producer
  completes writting them. Users specify such a dependency by using `depends`
  keyword in the workflow description.

# `separate_steps.yaml`
+ The explicit dependency between producer and consumer is removed in this case
  thanks to DYAD's dynamic dependency handling. Each of the producer and
  the consumer only depends on the startup step, and accesses the files on its
  local SSD. DYAD coordinates file accesses as well as the transfer from the
  producer's local storage to that of the consumer behind the scene.
  
# `single_step.yaml`
+ This is similar to the above case `separate_steps.yaml` except that the
  producer and the consumer are not separate jobs but different ranks in an
  MPI job. In usual implementation with MPI, an `MPI_Barrier` could have been
  used to synchronize the file accesses on a shared file system. However, in
  this case, it relies on DYAD.
