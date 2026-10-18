# Reader-lock handoff investigation

## Status

The first incremental implementation is present: whole-file consumer
coordination uses an OFD exclusive lock on `lock_fd`, and successfully opened
C and C++ application readers take an OFD shared lock for their own lifetime.
The regression tests verify that latter reader-lifetime behavior. They do not
cover the unresolved consume-to-reader handoff interval.

## Existing `dyad_consume()` descriptor sequence

`dyad_consume()` uses two internal descriptors for a whole-file request:

| Descriptor | Open | Purpose | Current completion behavior |
|---|---|---|---|
| `lock_fd` | `open(fname, O_RDWR | O_CREAT, 0666)` | Acquires the exclusive whole-file coordination lock; checks local state; prevents concurrent consumers from fetching/writing the same file. | Releases the exclusive lock, may invoke cache eviction, then closes the descriptor before returning. |
| `io_fd` | `open(fname, O_WRONLY)` | Writes received bytes through `dyad_cons_store()`. | Closed immediately after the store completes. |

The application descriptor is opened only after `dyad_consume()` returns:

- `dyad_open_wrapper()` calls the real `open()`.
- `dyad_fopen_wrapper()` calls the real `fopen()`.
- C++ stream wrappers call the underlying stream's `open()`.

## Remaining handoff gap

Taking a shared lock only after the application descriptor opens still leaves
this handoff interval unprotected:

1. `dyad_consume()` completes the fetch/write.
2. It releases `lock_fd`'s exclusive lock.
3. It may run `dyad_cache_maybe_evict()`.
4. It closes `lock_fd` and returns.
5. The wrapper opens the application reader descriptor.
6. A post-open shared lock would begin only here.

Consequently, another evictor can select the file after step 2 and before step
6.  The post-open lock cannot protect that interval.

## Candidate direction to investigate

Preserve `lock_fd` after a successful consume, downgrade its OFD exclusive
lock to an OFD shared reader guard, and transfer the guard only through the
wrapper's local call sequence. The wrapper would open the application
descriptor normally, take a shared OFD lock on it, then close `lock_fd`.
It remains a guard rather than the application reader descriptor because it
was opened `O_RDWR | O_CREAT`, whereas the application requested its own flags
and access mode.

This requires a short-lived guard-fd out parameter from the consume path to
the wrapper. On a successful real open, the wrapper takes its shared OFD lock
on the application descriptor before closing the guard. Error paths and failed
application opens must close the transferred guard.

## Locking constraint

OFD locks are owned by the descriptor's open file description. Therefore,
closing `io_fd` does not release `lock_fd`'s OFD exclusive lock, and closing a
separate application descriptor does not release a retained `lock_fd` guard.
Linux OFD and POSIX record locks conflict, so the OFD consumer lock still
coordinates with the producer's existing POSIX lock.

## Retained regressions

- `tests/stream/test_stream_reader_lock.cpp`: C++ filename-constructor and
  explicit-open reader checks.
- `tests/wrapper/test_reader_lock.c`: preloaded C wrapper checks for `open()`
  and `fopen()`.
- `tests/wrapper/consumer_test.c`: legacy wrapper-harness checks for the same
  acquire/release property.

Each opens a managed file, has a separate process attempt a nonblocking
exclusive POSIX lock while the reader remains open, then repeats after close.
The expected results are blocked while open and successful after close.
