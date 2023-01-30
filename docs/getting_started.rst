***************
Getting Started
***************

Prerequisites
#############

DYAD has the following minimum requirements to build and install:

* A C99-compliant C compiler
* A C++11-compliant C++ compiler
* Autoconf 2.63
* Automake
* Libtool
* Make
* pkg-config
* Jansson 2.10
* flux-core

Installation
############

Manual Installation
*******************

.. attention::

   Currently, DYAD can only be installed manually. This page will be updated as additional
   methods of installation are added.

.. note::

   Recommended for developers and contributors

You can get DYAD from its `GitHub repository <https://github.com/flux-framework/dyad>`_ using
these commands:

.. code-block:: shell

   $ git clone https://github.com/flux-framework/dyad.git
   $ cd dyad

DYAD uses the Autotools for building and installation. To start the build process, run
the following command to generate the necessary configuration scripts using Autoconf:

.. code-block:: shell

   $ ./auotgen.sh

Next, configure DYAD using the following command:

.. code-block:: shell

   $ ./configure --prefix=<INSTALL_PATH>

Besides the normal configure script flags, DYAD's configure script also has the following
flags:

+---------------------+-------------------------+--------------------------------------------+
| Flag                | Type (default)          | Description                                |
+=====================+=========================+============================================+
| --enable-dyad-debug | Bool (true if provided) | if enabled, include debugging prints and   |
|                     |                         | logs for DYAD at runtime                   |
+---------------------+-------------------------+--------------------------------------------+
| --enable-urpc       | Bool (true if provided) | if enabled, build the URPC sublibrary and  |
|                     |                         | Flux module                                |
+---------------------+-------------------------+--------------------------------------------+
| --enable-perfflow   | Bool (true if provided) | if enabled, build PerfFlow Aspect-based    |
|                     |                         | performance measurement annotations for    |
|                     |                         | DYAD                                       |
+---------------------+-------------------------+--------------------------------------------+

Finally, build and install DYAD using the following commands:

.. code-block:: shell

   $ make [-j]
   $ make install

Building with PerfFlow Aspect Support (Optional)
************************************************

DYAD has optional support for collecting cross-cutting performance data using
`PerfFlow Aspect <https://perfflowaspect.readthedocs.io/en/latest/>`_. To enable this support,
first build PerfFlow Aspect for C/C++ using
`their instructions <https://perfflowaspect.readthedocs.io/en/latest/BuildingPerfFlowAspect.html#c-build>`_.
Then, modify your method of choice for building DYAD as follows:

* **Manual Installation**: add :code:`--enable-perfflow` to your invocation of `./configure`

Running DYAD
############

TODO
