.. _dyad_getting_started:

***************
Getting Started
***************

.. _dyad_prerequisites:

Prerequisites
#############

DYAD has the following minimum requirements to build and install:

* A C11-compliant C compiler
* A C++17-compliant C++ compiler
* CMake 3.12
* pkg-config
* `flux-core <https://github.com/flux-framework/flux-core.git>`_
* `jansson 2.10 or newer <https://github.com/akheron/jansson.git>`_
* `gotcha 1.0.8 or newer <https://github.com/llnl/GOTCHA.git>`_
* flux-python

Optionally, DYAD leverages:

* `mochi-margo <https://github.com/mochi-hpc/mochi-margo.git>`_ to enable libfabric-based data transport layer (DTL).
* `ucx <https://github.com/openucx/ucx.git>`_ to enable ucx-based DTL.
* `dftracer <https://github.com/llnl/dftracer.git>`_ for performance tracing of Python-based applications.
* `cpp-logger <https://github.com/hariharan-devarajan/cpp-logger.git>`_ for logging operational details to aid debugging.
* `perflow-aspect <https://perfflowaspect.readthedocs.io/en/latest/>`_ for visualizing function-level events to aid workflow performance diagnosis.
* `caliper <https://github.com/llnl/Caliper.git>`_ for collecting performance profiling.


.. _dyad_installation:

Installation
############

Manual Installation
*******************

You can get DYAD from its `GitHub repository <https://github.com/flux-framework/dyad>`_ using
these commands:

.. code-block:: shell

   $ git clone https://github.com/flux-framework/dyad.git
   $ cd dyad

DYAD relies on cmake for both building and installation.

.. code-block:: shell

   $ mkdir build; cd build
   $ cmake -DDYAD_ENABLE_MARGO_DATA=ON \
           -DDYAD_LIBDIR_AS_LIB=ON \
           -DCMAKE_INSTALL_PREFIX=${DYAD_INSTALL_PREFIX} \
           ..
   $ make -j install

.. note::

   The cmake command above is provided as an example. Refer to the options below to customize configuration as needed.
   Set the env variable `DYAD_INSTALL_PREFIX` to the desired installation directory.


To enable the DYAD Python binding,

.. code-block:: shell

   $ python3 -m venv .venv
   $ source .venv/bin/activate
   $ pip install flux-python==0.80.0
   $ cd pydyad
   $ pip install .

.. note::

   When installing *flux-python*, ensure that the version matches *flux-core*.



There are several custom CMake options available to configure a DYAD build:

.. This is how the list-table below would render ideally
   +--------------------------+----------------------------+---------------------------------------------+
   | Flag                     | Values (**default**)       | Description                                 |
   +==========================+============================+=============================================+
   | DYAD_ENABLE_MARGO_DATA   | ON, **OFF**                | Allow dynamic selection of Margo-based DTL  |
   | DYAD_ENABLE_UCX_DATA     | ON, **OFF**                | Allow dynamic selection of UCX-based RMA DTL|
   | No option chosen         | NA                         | FLUX RPC-based DTL                          |
   +--------------------------+----------------------------+---------------------------------------------+
   | DYAD_LOGGER              | FLUX, CPP_LOGGER, **NONE** | Choose the method to log stdout/stderr      |
   | DYAD_LOGGER_LEVEL        | DEBUG, INFO, WARN,         | Choose the level of logging                 |
   |                          | ERROR, **NONE**            |                                             |
   +--------------------------+----------------------------+---------------------------------------------+
   | DYAD_PROFILER            | PERFFLOW_ASPECT, CALIPER,  | Choose the performance profiler             |
   |                          | DFTRACER, **NONE**         |                                             |
   +--------------------------+----------------------------+---------------------------------------------+
   | DYAD_ENABLE_TESTS        | ON, **OFF**                | Build unit tests                            |
   | DYAD_LIBDIR_AS_LIB       | ON, **OFF**                | Force lib as library install dir (no lib64) |
   | DYAD_USE_CLANG_LIBCXX    | ON, **OFF**                | Use clang's native runtime instead of gnu   |
   | DYAD_WARNINGS_AS_ERRORS  | ON, **OFF**                | Turn compiler warning into error            |
   +--------------------------+----------------------------+---------------------------------------------+
   | FLUX_CORE_PREFIX         | path_to_flux-core_install  | Allow cmake to find flux-core installed     |
   | GOTCHA_DIR               | path_to_gotcha_install     | Allow cmake to find gotcha installed        |
   +--------------------------+----------------------------+---------------------------------------------+


.. list-table::
   :header-rows: 1
   :widths: 25 25 50

   * - Flag
     - Values (**default**)
     - Description
   * - **Data Transfer Options**
     -
     -
   * - DYAD_ENABLE_MARGO_DATA
     - ON, **OFF**
     - Allow dynamic selection of Margo-based DTL
   * - DYAD_ENABLE_UCX_DATA
     - ON, **OFF**
     - Allow dynamic selection of UCX-based RMA DTL
   * - No option chosen
     - NA
     - FLUX RPC-based DTL
   * - **Logging and Profiling**
     -
     -
   * - DYAD_LOGGER
     - FLUX, CPP_LOGGER, **NONE**
     - Choose the method to log stdout/stderr
   * - DYAD_LOGGER_LEVEL
     - DEBUG, INFO, WARN, ERROR, **NONE**
     - Choose the level of logging
   * - DYAD_PROFILER
     - PERFFLOW_ASPECT, CALIPER, DFTRACER, **NONE**
     - Choose the performance profiler
   * - **Compiling/Linking Customization**
     -
     -
   * - DYAD_ENABLE_TESTS
     - ON, **OFF**
     - Build unit tests
   * - DYAD_LIBDIR_AS_LIB
     - ON, **OFF**
     - Force lib as library install dir (no lib64)
   * - DYAD_USE_CLANG_LIBCXX
     - ON, **OFF**
     - Use clang's native runtime instead of GNU
   * - DYAD_WARNINGS_AS_ERRORS
     - ON, **OFF**
     - Turn compiler warnings into errors
   * - **Core Dependency Paths**
     -
     -
   * - FLUX_CORE_PREFIX
     - path_to_flux-core_install
     - Allow cmake to find flux-core installed
   * - GOTCHA_DIR
     - path_to_gotcha_install
     - Allow cmake to find gotcha installed


.. note::

   **mochi-margo** enables seamless adoption of various DTL types. When installed
   via spack, it supports **libfabric** by default, but not UCX. To enable UCX with
   Margo, Mercury (on which Margo depends) must be built with UCX support enabled.
   Mercury required UCX configure to support multi-threading. That means UCX is
   installed with either with the Spack option ``+thread_multiple`` or with
   UCX build config ``--enable-mt``.
   One way to achieve this is to use a Spack environment.

   .. code-block:: bash

      spack env create dyad_margo
      spack env activate dyad_margo
      spack add mercury +ofi +ucx +sm
      spack add mochi-margo
      spack concretize --force
      spack install


   On LC Corona, to leverage the existing installations of UCX and libfabric under
   system directories, edit the **spack.yaml** of the Spack environment via the command
   ``spack config edit packages`` and add the following:

   .. code-block:: yaml

      spack:
        specs:
        - mochi-margo
        - mercury@2.4.1+ofi+sm+ucx ^ucx@1.17.0 ^libfabric@2.3.1
        view: true
        concretizer:
          unify: true
        packages:
          libfabric:
            externals:
            - spec: "libfabric@2.3.1 fabrics=verbs,tcp,udp,sockets"
              prefix: /usr
            buildable: false
          cmake:
            externals:
            - spec: cmake@3.30.5
              prefix: /usr/tce/packages/cmake/cmake-3.30.5
              modules:
              - cmake/3.30.5
            buildable: false
            require: cmake@3.30.5
          ucx:
            externals:
            - spec: ucx@1.17.0 +thread_multiple +cma +cuda +gdrcopy +knem +rdmacm +verbs +dc +rc +ud +mlx5_dv
              prefix: /usr
            buildable: false


   To enable a specific DTL type, DYAD requires the environment variable
   ``DYAD_DTL_MODE`` to be set accordingly. At present, three values are
   supported: ``MARGO``, ``UCX``, and ``FLUX_RPC``.

   - When ``DYAD_DTL_MODE`` is set to ``UCX`` and DYAD has been built with the
     CMake option ``DYAD_ENABLE_UCX_DATA=ON``, data transfer is performed
     asynchronously via **remote memory access (RMA)** to reduce communication
     costs.
   - For details on choosing a network protocol with MARGO, see :doc:`runtime_configuration`.

   However, the selection between ``MARGO``, ``UCX``, and ``FLUX_RPC`` can be
   made dynamically at launch time. Ensure that ``DYAD_DTL_MODE`` is set
   consistently in both the service and client environments.

   If neither optional DTL CMake option is enabled, DYAD uses **FLUX RPC** for
   data transfer. All three DTLs use Flux RPC for the control request from a
   consumer to the producer service; UCX and Margo use their own data path
   after that request is received.

   **flux-core** is required to build DYAD. cmake build proceedure for dyad will
   attempt to find it under the path ``FLUX_CORE_PREFIX`` if it is set.
   Otherwise, it searches flux-core through user's ``PATH``.

   **gotcha** allows consistent symbol interception behavior when there are multiple
   interception libraries at runtime. For example, both Spindle and DYAD intercept
   :code:`open()` for different purposes. gotcha chains the wrappers so both
   execute, with execution order controlled by user-defined priority.
   gotchat is required to build DYAD but is only relevant to the interception wrapper.


.. _dyad_user_api:

Using DYAD's APIs
#################

Currently, DYAD provides APIs for the following programming languages:

* C
* C++
* Python

This section describes the basics of integrating them into an application.

.. _dyad_user_api_c:

C API
*****

DYAD's C API leverages the `LD_PRELOAD trick <https://www.admin-magazine.com/HPC/Articles/Preload-Trick>`_
to integrate into user applications. As a result, users can utilize DYAD's C API by
simply adding the following before the shell command that launches their application:

.. code-block:: shell

   $ LD_PRELOAD=/path/to/libdyad_wrapper.so your-application

Once preloaded, DYAD's C API will intercept the :code:`open` and :code:`fopen` functions when consuming
files and the :code:`close` and :code:`fclose` functions when producing files.
(Similar with :code:`open64`, :code:`fopen64`, :code:`close64` and :code:`fclose64`)
As a result, if their code already uses thse functions, users do not need to change their code.

GOTCHA allows consistent symbol interception behavior when there are multiple
interception libraries at runtime by controlling the execution order by priority.
DYAD's priority can be set via the ``DYAD_GOTCHA_PRIORITY`` environment variable
— a higher integer value means DYAD's wrapper runs first. If the variable is not
set, all tools default to priority 0 and order is determined by load order.

.. _dyad_user_api_cpp:

C++ API
*******

DYAD's C++ API is implemented as a small library that wraps C++'s Standard Library file streams.
LD_PRELOAD-based function substitution does not apply to C++ streams because the
symbols to be intercepted are not exposed. Instead, we provide a set of lightweight
wrapper classes. To use DYAD's C++ API, first, add the following to your code:

.. code-block:: cpp

   #include <dyad/stream/dyad_stream_api.hpp>

This header defines thin wrappers around the file streams provided by the C++ Standard Library.
More specifically, it provides the following classes:

* :code:`dyad::basic_ifstream_dyad`
* :code:`dyad::ifstream_dyad`
* :code:`dyad::wifstream_dyad`
* :code:`dyad::basic_ofstream_dyad`
* :code:`dyad::ofstream_dyad`
* :code:`dyad::wofstream_dyad`
* :code:`dyad::basic_fstream_dyad`
* :code:`dyad::fstream_dyad`
* :code:`dyad::wfstream_dyad`

When using DYAD, these file streams should be used in place of the file streams from the C++
Standard Library. The DYAD file streams should be directly used to do the following:

* Open files (with the file stream's :code:`open` method)
* Close files (with the file stream's :code:`close` method or destructor)
* Access the underlying C++ Standard Library file stream using the DYAD stream's :code:`get_stream` method

All reading from and writing to files should be done using the underlying C++ Standard Library file stream.
A simple example of using DYAD's C++ API in a producer application is shown below:

.. code-block:: cpp

   #include <dyad/stream/dyad_stream_api.hpp>

   void produce_file(std::string& full_path, int32_t* data, std::size_t data_size)
   {
       dyad::ofstream_dyad ofs_dyad;
       ofs_dyad.open(full_path, std::ofstream::out | std::ios::binary);
       std::ofstream& ofs = ofs_dyad.get_stream();
       ofs.write((char*) data, data_size);
       ofs_dyad.close();
   }

After replacing C++ Standard Library file streams with their DYAD equivalents,
there is one final requirement to using the C++ API. When compiling your code,
you must link the associated library (i.e., :code:`libdyad_fstream.so` or
:code:`libdyad_fstream.a`). This library can be found in the :code:`lib`
subdirectory of the install prefix.


.. _dyad_user_api_python:

Python
*******

We offer `PyDYAD <https://github.com/flux-framework/dyad/tree/main/pydyad/pydyad>`_, a Python binding to the DYAD client library implemented in C.
A producer-consumer example can be found at `tests/pydyad_spsc <https://github.com/flux-framework/dyad/tree/main/tests/pydyad_spsc>`_.

.. toctree::
   :maxdepth: 1
   :caption: An exmaple of integrating DYAD with PyTorch DataLoader

   integration_with_PyTorch


Running DYAD
############

There are three steps to running DYAD-enabled applications:

1. :ref:`Create a Flux key-value store (KVS) namespace <DYAD_Flux_KVS_NS>`
2. :ref:`Determine the managed directories for each application <DYAD_managed_dir>`
3. :ref:`Load DYAD's Flux module <DYAD_user_service>`
4. :ref:`Configure and run the DYAD-enabled applications <DYAD_enabling_apps>`

.. _DYAD_Flux_KVS_NS:

Create a Flux KVS Namespace
***************************

DYAD uses its own namespace in Flux's hierarchical key-value store (KVS) to isolate
itself from the KVS entries from other Flux services. Thus, the first step in running DYAD
is to create a KVS namespace. This namespace is used by DYAD to exchange
file information (e.g., the Flux broker that "owns" a file) needed to synchronize
the consumer application and transfer the file from producer to
consumer. To create this namespace, run the following:

.. code-block:: shell

   $ flux kvs namespace create <DYAD_KVS_NAMESPACE>

The namespace can be whatever string value you want.


.. _DYAD_managed_dir:

Determine the Managed Directories for Each Application
******************************************************

To determine when to perform synchronization and data transfer, DYAD tracks two directories for
each application: a **producer-managed directory** and a **consumer-managed directory**. At least
one of these directories must be specified for DYAD to do anything. If neither are provided, the application
will still run, but DYAD will not do anything.

When a producer-managed directory is provided, DYAD will store information about any file
stored in that directory (or its subdirectories) into a namespace within the Flux key-value
store (KVS). This information is later used by DYAD to transfer files from producer to consumer.

When a consumer-managed directory is provided, DYAD will block the application whenever a
file inside that directory (or subdirectory) is opened. This blocking will last until DYAD sees
information about the file in the Flux KVS namespace. If the information retrieved from the KVS
indicates that the file is actually located elsewhere, DYAD will use Flux's
remote procedure call (RPC) system to ask DYAD's Flux module to transfer the file.
If a transfer occurs, the file's contents will be stored at the file path passed to the
original file opening function (e.g., :code:`open`, :code:`fopen`).

Before running the following steps, determine the producer- and/or consumer-managed directories
for each application. These directories will need to be provided to the commands in the next steps.

.. note::

   When opening or closing a file not in the producer- or consumer-managed directories, DYAD
   will simply open or close the file. DYAD changes the behavior of opening or closing only the
   files in the managed directories.


.. _DYAD_user_service:

Load DYAD's Flux Module
***********************

The next step in running DYAD is to load DYAD's Flux module. The module is the component of DYAD
responsible for sending files from producer to consumer. Once loaded, this module will run whenever
its associated Flux broker receives a relevant remote procedure call from a DYAD-enabled consumer. To load the module,
first, determine where :code:`dyad.so` is located. This should normally be :code:`<PREFIX>/lib/dyad.so`. Once you
have found the path to :code:`dyad.so`, you can load the module on the current Flux broker using:

.. code-block:: shell

   $ dyad start -p <DYAD_PATH_PRODUCER> --mode <FLUX_RPC|UCX|MARGO>

The installed ``dyad`` command loads the module on all Flux brokers. Its
``start`` command requires ``-p``/``--producer_managed_path`` and accepts
``-m``/``--mode`` for ``FLUX_RPC`` (default), ``UCX``, or ``MARGO``. It also
accepts ``-o``/``--origin_path`` to configure lazy origin-backed range fills.
The producer-managed directory is the root from which the module looks for
files to transfer.

``dyad start`` uses ``flux exec -r all`` so the module is loaded on every
broker in the current Flux instance. For manual loading, the module path is
normally ``<prefix>/lib/dyad.so``; loading it on only one broker serves only
requests routed to that broker. The same module options may be supplied to
``flux module load``.


.. _DYAD_enabling_apps:
   
Configure and Run the DYAD-Enabled Applications
***********************************************

In addition to the two essential variables discussed above—``DYAD_KVS_NAMESPACE``
and ``DYAD_DTL_MODE``—two more variables, ``DYAD_PATH_PRODUCER`` and ``DYAD_PATH_CONSUMER``,
are required to run DYAD-enabled applications.”
The table below list the essential ones. For the full list of variables, refer to
:doc:`runtime_configuration`.


+--------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| Name                           | Type            | Required?    | Default  | Description                                                     |
+================================+=================+==============+==========+=================================================================+
| :code:`DYAD_KVS_NAMESPACE`     | String          | Yes          | N/A      | The Flux KVS namespace that DYAD will use to record or look     |
|                                |                 |              |          |                                                                 |
|                                |                 |              |          | for file information                                            |
+--------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_PATH_PRODUCER`     | Directory Path  | Yes [#one]_  | N/A      | The producer-managed path of the application                    |
+--------------------------------+                 +              +          +-----------------------------------------------------------------+
| :code:`DYAD_PATH_CONSUMER`     |                 |              |          | The consumer-managed path of the application                    |
+--------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+
| :code:`DYAD_DTL_MODE`          | String          | No           | FLUX_RPC | Choose data transfer method among MARGO, UCX, FLUX_RPC          |
+--------------------------------+-----------------+--------------+----------+-----------------------------------------------------------------+

.. [#one] For DYAD to do anything, at least one of :code:`DYAD_PATH_PRODUCER` or :code:`DYAD_PATH_CONSUMER` must be provided.
   Applications will still work if neither are provided, but DYAD will not do anything.

