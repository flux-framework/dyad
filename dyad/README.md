DYAD: DYnamic and Asynchronous Data Streamliner

DYAD aims to help sharing data files between producer and consumer job elements,
especially within an ensemble or between co-scheduled ensembles.
DYAD provides the service by two components: a FLUX module and a I/O wraper set.
DYAD transparently synchronizes file I/O between producer and consumer, and
transfers data from the producer location to the consumer location managed by the service.
Users only need to use the file path that is under the directory managed by the service.

### License

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420


### Information on the license of the external projects on which this project depends

- MURMUR3 - License and contributing - All this code is in the public domain. Murmur3 was created by Austin Appleby, and the C port and general tidying up was done by Peter Scott. If you'd like to contribute something, I would love to add your name to this list.

- LIBB64 - License: This work is released into the Public Domain. It basically boils down to this: I put this work in the public domain, and you can take it and do whatever you want with it. An example of this "license" is the Creative Commons Public Domain License, a copy of which can be found in the LICENSE file in the distribution, and also on-line at http://creativecommons.org/licenses/publicdomain/
