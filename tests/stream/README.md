# How to compile and run tests/stream

## 1. Build and install DYAD

Set `DYAD_INSTALL` to the DYAD install location and add the `dyad` binary to your PATH:

```bash
export DYAD_INSTALL=/path/to/dyad/install
export PATH=${DYAD_INSTALL}/bin:${PATH}
```

## 2. Compile test_stream

```bash
cd tests/stream
g++ -std=c++14 -g -O0 \
    -DDYAD_HAS_CONFIG \
    -I${DYAD_INSTALL}/include \
    test_stream.cpp \
    -L${DYAD_INSTALL}/lib \
    -ldyad_fstream \
    -o test_stream
```

## 3. Start a Flux session and the DYAD service

```bash
export DYAD_PATH=/tmp/${USER}/dyad
flux start --test-size=2
```

Inside the Flux session, create the KVS namespace and start the DYAD service:

```bash
flux kvs namespace create test
dyad start -p ${DYAD_PATH}
```

## 4. Run the test

`test_stream` takes 4 arguments: `<dyad_path> <file> <write=1|read=0> <use_open_close=1|0>`

```bash
# Producer (writer)
flux run -n 1 ./test_stream ${DYAD_PATH} ${DYAD_PATH}/test.txt 1 0

# Consumer (reader) — run after producer
flux run -n 1 ./test_stream ${DYAD_PATH} ${DYAD_PATH}/test.txt 0 0
```

## 5. Stop the DYAD service and exit

```bash
flux kvs namespace remove test
dyad stop
exit
```

---

**Note:** The stream tests are not integrated into CMake/ctest. They are manual
integration tests that require a live Flux environment with the DYAD module loaded.
`combined_test.cpp` and `io_test.cpp` support test logic (not standalone executables),
while `test_stream.cpp` is the main entry point.

## Reader-lock regression

Compile `test_stream_reader_lock.cpp` with the same command as `test_stream.cpp`.
With the DYAD service running, first publish then consume:

```bash
flux run -n 1 ./test_stream_reader_lock ${DYAD_PATH} producer
flux run -n 1 ./test_stream_reader_lock ${DYAD_PATH} consumer
```

The consumer checks both the explicit `open()` path and the constructor path.
While each `ifstream_dyad` is open, a separate process must be unable to take
an exclusive POSIX lock; after `close()`, it must succeed.

## C interception-wrapper reader-lock regression

Compile `tests/wrapper/test_reader_lock.c`, then run it with
`libdyad_wrapper.so` preloaded after the producer step above:

```bash
cc -std=c11 -Wall -Wextra -pedantic -DDYAD_HAS_CONFIG \
  -I${DYAD_SOURCE}/include -I${DYAD_SOURCE}/src -I${DYAD_BUILD}/include \
  tests/wrapper/test_reader_lock.c -L${DYAD_BUILD}/lib \
  -Wl,-rpath,${DYAD_BUILD}/lib -ldyad_client -ldyad_utils -ldyad_ctx -o test_reader_lock
DYAD_PATH_PRODUCER=${DYAD_PATH}/ DYAD_PATH_CONSUMER=${DYAD_PATH}/ \
  DYAD_KVS_NAMESPACE=dyad_stream_reader_lock \
  LD_PRELOAD=${DYAD_INSTALL}/lib/libdyad_wrapper.so \
  ./test_reader_lock ${DYAD_PATH}/reader_lock_open.txt
```

It makes the same acquire-and-release checks for intercepted C `open()` and
`fopen()` calls. The competing process bypasses interception deliberately so
it measures the parent reader's lock rather than taking another reader lock.
