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
