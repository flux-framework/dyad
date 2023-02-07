.. DYAD documentation master file, created by
   sphinx-quickstart on Thu Jan 26 10:12:01 2023.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

DYAD: Dynamic and Asynchronous Data Streamliner
===============================================

DYAD is a synchronization and data movement tool for computational science workflows built
on top of Flux. DYAD aims to provide the benefits of in situ and in transit tools
(e.g., fine-grained synchronization of consumer applications, fast data access due to spatial locality)
while relying on a file-based data abstraction to maximize portability and minimize code change
requirements for workflows. More specifically, DYAD aims to overcome the following challenges
associated with traditional shared-storage and modern in situ and in transit data movement approaches:

* Lack of per-file or per-data object synchronization in shared-storage approaches
* Poor temporal and spatial locality in shared-storage approaches
* Poor performance for file/data object metadata operations in shared-storage approaches (and possibly some in situ and in transit approaches)
* Poor portability and the introduction of required code changes for in situ and in transit approaches

In resolving these challenges, DYAD aims to provide the following to users:

* Good performance (similar to in situ and in transit) due to on- or near-node temporary storage of data
* Per-file or per-data object synchronization of consumer applications
* Little to no code change to existing workflows to achieve the previous benefits

.. toctree::
   :maxdepth: 2
   :caption: User Docs:

   getting_started

.. toctree::
   :maxdepth: 2
   :caption: Tutorials

   ecp_feb_2023_tutorial

.. toctree::
   :maxdepth: 2
   :caption: References

   publications

.. toctree::
   :maxdepth: 2
   :caption: Contributing

   developer_guide

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
