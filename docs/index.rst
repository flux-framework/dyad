.. DYAD documentation master file, created by
   sphinx-quickstart on Thu Jan 26 10:12:01 2023.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

DYAD: Dynamic and Asynchronous Data Streamliner
===============================================

DYAD aims to help sharing data files between producer
and consumer job elements, especially within an ensemble
or between co-scheduled ensembles. DYAD provides the service
by two components: a Flux module and an API that wraps
conventional file I/O. DYAD transparently synchronizes file I/O
between producer and consumer and transfers data from the producer
location to the consumer location managed by the service. With DYAD,
users only need to use the file path that is under the directory
managed by the service.


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
