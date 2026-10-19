# DYAD Changelog

---

## [Unreleased] — 2026-05-19

### Summary

Three pull requests (PR #161, #162, #163) landed a series of related changes:
simplifying the UCX data transport layer, fixing a critical crash in FLUX_RPC
and Margo modes introduced by that simplification, hardening the Python bindings,
and cleaning up the CI test scripts.

---

## PR #163 — Make dftracer an optional dependency in pydyad

**Commits:** `f506aba`, `95b3b87`

### Changes

- `pydyad/pydyad/bindings.py`, `pydyad/pydyad/context.py`: Wrapped the
  `dftracer` import in `try/except ImportError` blocks and replaced the missing
  module with no-op stubs. pydyad now works in environments without dftracer
  installed.
- `.github/workflows/compile_test.yaml`: Restored `numpy` to the system-level
  `pip3 install` command (`sudo pip3 install jsonschema cffi ply pyyaml numpy`).

**Rationale:** dftracer is an optional profiling library. Its absence was causing
import failures and preventing pydyad from loading at all, even when profiling
was not needed. On the CI side, Flux 0.73.0 jobs inherit the system environment
rather than the virtual environment activated in prior steps, so numpy must be
installed into the system Python, not just the venv.

---

## PR #161 — Use dyad CLI in CI scripts and fix dyad_exe start/stop

**Commits:** `1510654`, `f299c5d`, `3bea651`, `704216e`

### Changes

- `src/dyad/service/dyad_exe.c`: Rewrote and consolidated the `dyad` CLI.
  `dyad start -p <path> [-m <mode>]` loads the DYAD Flux module on all brokers;
  `dyad stop` unloads it. The `-m` flag selects the DTL mode at runtime (default:
  `FLUX_RPC`).
- `.github/workflows/compile_test.yaml`:
  - Expanded the test matrix to include all three DTL modes: `UCX`, `FLUX_RPC`,
    and `MARGO`.
  - Gated the UCX and Margo Spack install/load steps behind their respective
    `DYAD_DTL_MODE` matrix values so each CI job only installs what it needs.
- `.github/prod-cons/dyad_prod_cons_test.sh`, `dyad_producer.sh`: Updated CI
  orchestration scripts to use the `dyad` CLI (`dyad start`/`dyad stop`) instead
  of invoking `flux module load` directly.
- `tests/stream/README.md`: Minor fix to KVS namespace setup instructions.

**Rationale:** Previously, the CI only tested one DTL mode (UCX). The three-mode
matrix catches regressions across all supported transports. Centralizing service
lifecycle into the `dyad` CLI makes scripts simpler and more consistent.

---

## PR #162 — Simplify UCX DTL to always use RMA + bug fixes

**Commits:** `7cfbdb4`, `0a85a5f`, `25c2d85`, `a7098e3`, `fc2b3ce`, `05a4349`,
`9246400`, `61bee62`, `5bbee7e`, `540135c`

This PR landed in two phases: the UCX simplification itself, then a series of
fixes and CI polish triggered by CI failures it exposed.

### Phase 1 — UCX simplification (`7cfbdb4`)

**Files:** `src/dyad/dtl/ucx_dtl.c`, `src/dyad/client/dyad_client.c`,
`src/dyad/service/flux_module/dyad.c`, `CMakeLists.txt`,
`src/dyad/dtl/CMakeLists.txt`, `src/dyad/dtl/dyad_dtl_api.c`

Removed `DYAD_ENABLE_UCX_DATA_RMA` as a separate CMake and preprocessor option.
UCX now unconditionally uses one-sided RDMA (RMA). Deleted ~275 lines of dead
code from `ucx_dtl.c`:

- The tag-send/tag-recv code path (including `dyad_recv_callback`,
  `ucp_tag_send_nbx`/`nb`, `ucp_tag_recv_nbx` probe loops).
- All `#ifndef DYAD_ENABLE_UCX_RMA` / `#else` / `#endif` guards around those
  paths.

**Rationale:** The tag-based send/recv path had been superseded by RMA for
correctness and performance reasons. Keeping two code paths was a maintenance
burden and source of confusion. UCX RMA is the only transport that makes sense
for DYAD's one-sided data push model.

### Phase 2 — Cleanup: remove residual macros (`0a85a5f`)

**Files:** `CMakeLists.txt`, `cmake/configure_files/dyad_config.hpp.in`,
`src/dyad/dtl/dyad_dtl_api.c`

- Removed `set(DYAD_ENABLE_UCX_RMA 1)` from `CMakeLists.txt` (nothing reads
  this variable anymore).
- Removed `#cmakedefine DYAD_ENABLE_UCX_RMA 1` from `dyad_config.hpp.in`.
- Replaced stale `|| defined(DYAD_ENABLE_UCX_DATA_RMA)` conditions in
  `dyad_dtl_api.c` with the canonical `defined(DYAD_ENABLE_UCX_DTL)`.

### Phase 3 — Critical bug fix: FLUX_RPC and Margo consumer crash (`25c2d85`)

**Files:** `src/dyad/service/flux_module/dyad.c`,
`src/dyad/client/dyad_client.c`

**Root cause:** The original UCX simplification commit inadvertently removed the
`#ifdef DYAD_ENABLE_UCX_RMA` guards around the size-prefix buffer encoding,
making it run unconditionally for all DTL modes.

UCX RMA has no built-in message framing, so DYAD's server prepends
`file_size` as a `ssize_t` (8 bytes) before the file data, and the client
strips it after the RDMA transfer. FLUX_RPC and Margo carry the data length
natively in their transport envelopes and do not need this prefix.

Without the guards, two bugs manifested for FLUX_RPC and Margo:

1. **Server (dyad.c) — buffer overflow:** `get_buffer` allocates exactly
   `file_size` bytes, but the server wrote `file_size + 8` bytes
   (file data plus the size prefix). On small test files the overflow was
   masked by `posix_memalign` page-aligned padding; on larger files or with
   strict heap checking (Ubuntu 24.04) it was caught.

2. **Client (dyad_client.c) — heap corruption → SIGABRT:** After the transfer,
   the client unconditionally advanced `*file_data` by `sizeof(ssize_t)` (8
   bytes) and then called `return_buffer`, which calls `free()` on that offset
   pointer. Freeing a pointer that is 8 bytes past the start of a
   `posix_memalign` allocation triggers `abort()` in glibc. This caused the
   consumer to crash with SIGABRT after successfully writing the first file.

**Fix:** Restored the guards using `DYAD_ENABLE_UCX_DTL` (replacing the old
`DYAD_ENABLE_UCX_RMA` macro, since UCX now always uses RMA). On the server side,
introduced a `buf_offset` variable to avoid duplicating the `read()` loop:

```c
#ifdef DYAD_ENABLE_UCX_DTL
    const size_t buf_offset = sizeof (file_size);
#else
    const size_t buf_offset = 0;
#endif
```

The size prefix is written and the read buffer is offset only when building with
UCX support. On the client side, the extraction of the size prefix and the
pointer advance are likewise guarded by `#ifdef DYAD_ENABLE_UCX_DTL`.

### CI fixes

- **`a7098e3` / `fc2b3ce` / `9246400`** — Fixed `numpy` not found in Python
  flux jobs. Two issues:
  1. Flux 0.73.0 jobs do not inherit the activated venv PATH, so they use system
     `python3`. numpy must be installed into the system Python via `sudo pip3
     install`.
  2. The `PYTHONPATH` used a bash glob (`python3*/site-packages`) in a shell
     variable assignment. Bash does not expand globs in assignments, so Python
     received a literal `*` in the path. Fixed by computing the exact version
     string at runtime:
     ```bash
     PYTHON_VER=$(python3 -c "import sys; print(f'python{sys.version_info.major}.{sys.version_info.minor}')")
     export PYTHONPATH=...:${DYAD_INSTALL_PREFIX}/lib/${PYTHON_VER}/site-packages
     ```
- **`05a4349`** — Increased per-test timeout from 1 min to 2 min to reduce
  spurious timeouts on loaded GitHub-hosted runners.
- **`61bee62`** — Moved `dyad start` out of `dyad_producer.sh` into the
  top-level orchestration script `dyad_prod_cons_test.sh`. The service
  is logically independent of the producer job and should be running before
  any jobs are submitted.
- **`5bbee7e`** — Inlined `prod_cons_argparse.sh` into `dyad_prod_cons_test.sh`
  and deleted the now-empty helper file.
- **`540135c`** — Removed unused `this_script_dir` variable from
  `dyad_prod_cons_test.sh`.

---

## How to Use DYAD

### Prerequisites

- Flux resource manager running (`flux start` or a real allocation).
- A Flux KVS namespace for coordination.
- DYAD installed (see Build section in CLAUDE.md or README).

### Environment variables

| Variable | Required | Description |
|---|---|---|
| `DYAD_KVS_NAMESPACE` | Yes | Flux KVS namespace used for file metadata. Must be created before starting the service. |
| `DYAD_PATH_PRODUCER` | Yes (producer job) | Directory where the producer writes files. DYAD monitors this directory. |
| `DYAD_PATH_CONSUMER` | Yes (consumer job) | Directory where the consumer expects to read files. |
| `DYAD_DTL_MODE` | No | Data transport: `FLUX_RPC` (default), `UCX`, or `MARGO`. |

Optional tuning variables (all have defaults):

| Variable | Description |
|---|---|
| `DYAD_SHARED_STORAGE` | Set to `1` if producer and consumer share a filesystem. Skips data transfer. |
| `DYAD_ASYNC_PUBLISH` | Set to `1` to publish file metadata asynchronously after write. |
| `DYAD_FSYNC_WRITE` | Set to `1` to fsync after each producer write. |
| `DYAD_KEY_DEPTH` | Number of path components used as the KVS key prefix (default: 1). |
| `DYAD_KEY_BINS` | Number of KVS key bins (default: 1). |
| `DYAD_MARGO_PROTO` | Margo protocol string, e.g. `ofi+tcp` (Margo mode only). |

### 1. Start a Flux session (if not already in one)

```bash
flux start --test-size=2
```

### 2. Create a KVS namespace

```bash
flux kvs namespace create ${DYAD_KVS_NAMESPACE}
```

### 3. Start the DYAD service

```bash
dyad start -p <producer_managed_path> [-m FLUX_RPC|UCX|MARGO]
```

`dyad start` runs `flux module load dyad.so` on every broker. The `-p` flag is
mandatory and tells the service which directory to watch for new files. `-m`
selects the data transport (default: `FLUX_RPC`).

### 4. Run producer and consumer jobs

**C/C++ applications using the GOTCHA wrapper (transparent interception):**

```bash
# Producer job — LD_PRELOAD intercepts fopen/fclose transparently
flux submit --nodes 1 --exclusive \
    --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} \
    --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} \
    --env=DYAD_PATH_PRODUCER=${DYAD_PATH_PRODUCER} \
    env LD_PRELOAD=<install_prefix>/lib/libdyad_wrapper.so \
    ./my_producer ${DYAD_PATH_PRODUCER}

# Consumer job
flux submit --nodes 1 --exclusive \
    --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} \
    --env=DYAD_DTL_MODE=${DYAD_DTL_MODE} \
    --env=DYAD_PATH_CONSUMER=${DYAD_PATH_CONSUMER} \
    env LD_PRELOAD=<install_prefix>/lib/libdyad_wrapper.so \
    ./my_consumer ${DYAD_PATH_CONSUMER}
```

**C++ applications using the Stream API directly:**

```cpp
#include <dyad/stream/dyad_stream_api.hpp>

// Producer: write a file — DYAD publishes metadata automatically on close
dyad::ofstream_dyad out;
out.open(filepath, std::ios::out | std::ios::binary);
out.get_stream() << data;
out.close();

// Consumer: read a file — DYAD fetches data automatically on open
dyad::ifstream_dyad in;
in.open(filepath, std::ios::in | std::ios::binary);
in.get_stream() >> data;
in.close();
```

**Python applications using pydyad:**

```python
from pydyad import Dyad
from pydyad.context import dyad_open

# Explicit context
dyad_ctx = Dyad()
dyad_ctx.init_env()  # reads DYAD_* env vars

# Producer
with dyad_open("/path/to/producer_dir/file.dat", mode="w", dyad_ctx=dyad_ctx) as f:
    f.write(data)

# Consumer
with dyad_open("/path/to/consumer_dir/file.dat", mode="r", dyad_ctx=dyad_ctx) as f:
    data = f.read()
```

`dyad_open` is a drop-in replacement for Python's built-in `open`. It
automatically calls `produce`/`consume` on close/open based on whether the file
path is under the producer or consumer managed directory.

### 5. Stop the DYAD service

```bash
dyad stop
flux kvs namespace remove ${DYAD_KVS_NAMESPACE}
```

### 6. Full example (Flux session, FLUX_RPC mode)

```bash
export DYAD_KVS_NAMESPACE=test
export DYAD_DTL_MODE=FLUX_RPC
export DYAD_PATH_PRODUCER=/tmp/dyad/producer
export DYAD_PATH_CONSUMER=/tmp/dyad/consumer

flux start --test-size=2 bash -c '
  flux kvs namespace create ${DYAD_KVS_NAMESPACE}

  dyad start -p ${DYAD_PATH_PRODUCER}

  flux submit --nodes 1 --exclusive \
      --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} \
      --env=DYAD_PATH_CONSUMER=${DYAD_PATH_CONSUMER} \
      my_consumer ${DYAD_PATH_CONSUMER}

  flux submit --nodes 1 --exclusive \
      --env=DYAD_KVS_NAMESPACE=${DYAD_KVS_NAMESPACE} \
      --env=DYAD_PATH_PRODUCER=${DYAD_PATH_PRODUCER} \
      env LD_PRELOAD=<install>/lib/libdyad_wrapper.so \
      my_producer ${DYAD_PATH_PRODUCER}

  flux job attach $(flux job last)

  dyad stop
  flux kvs namespace remove ${DYAD_KVS_NAMESPACE}
'
```

### DTL mode selection guide

| Mode | When to use | Extra requirement |
|---|---|---|
| `FLUX_RPC` | Default. Works everywhere Flux is available. | None |
| `UCX` | High-bandwidth, low-latency networks (InfiniBand, RoCE). | UCX library; build with `-DDYAD_ENABLE_UCX_DATA=ON` |
| `MARGO` | HPC fabrics using the Mercury/Margo stack (OFI, CXI). | mochi-margo; build with `-DDYAD_ENABLE_MARGO_DATA=ON` |

---

## Build quick reference

```bash
mkdir build && cd build

# FLUX_RPC only (default, no extra deps)
cmake -DCMAKE_INSTALL_PREFIX=<prefix> \
      -DDYAD_ENABLE_TESTS=ON \
      -DDYAD_LOGGER=FLUX -DDYAD_LOGGER_LEVEL=INFO \
      ..

# With UCX support
cmake -DCMAKE_INSTALL_PREFIX=<prefix> \
      -DDYAD_ENABLE_UCX_DATA=ON \
      -DDYAD_LOGGER=FLUX -DDYAD_LOGGER_LEVEL=INFO \
      ..

# With Margo support
cmake -DCMAKE_INSTALL_PREFIX=<prefix> \
      -DDYAD_ENABLE_MARGO_DATA=ON \
      -DDYAD_LOGGER=FLUX -DDYAD_LOGGER_LEVEL=INFO \
      ..

make install -j$(nproc)
```

After installation, `<prefix>/bin/dyad` is the service CLI and
`<prefix>/lib/libdyad_wrapper.so` is the GOTCHA wrapper for transparent
interception.
