from pydyad.bindings import Dyad

from contextlib import contextmanager
import io
from pathlib import Path

try:
    from dftracer.python import dft_fn
    dft_log = dft_fn("DYAD_PY")
except ImportError:
    class _NoopLog:
        def log(self, fn):
            return fn
    dft_log = _NoopLog()
DYAD_IO = None


# The design of dyad_open is based on the PEP for Python's 'with' syntax:
# https://peps.python.org/pep-0343/
@contextmanager
def dyad_open(*args, dyad_ctx=None, metadata_wrapper=None, register_dyad_ctx=False, **kwargs):
    """Open a file with DYAD synchronization.

    A managed output is published only after the caller's ``with`` body and
    the underlying file close both complete successfully. An exception from
    the body closes the file without publishing it to consumers.

    Args:
        *args: Arguments forwarded to :func:`io.open`.
        dyad_ctx: Optional initialized DYAD context.
        metadata_wrapper: Optional previously fetched DYAD metadata.
        register_dyad_ctx: Whether to register ``dyad_ctx`` as the default.
        **kwargs: Keyword arguments forwarded to :func:`io.open`.

    Yields:
        The file object returned by :func:`io.open`.
    """
    global DYAD_IO
    local_dyad_io = dyad_ctx
    if dyad_ctx is None:
        if DYAD_IO is None:
            DYAD_IO = Dyad()
            DYAD_IO.init_env()
        local_dyad_io = DYAD_IO
    else:
        if register_dyad_ctx:
            DYAD_IO = dyad_ctx
    fname = args[0]
    if not isinstance(args[0], Path):
        fname = Path(args[0])
    fname = fname.expanduser().resolve()
    mode = None
    if "mode" in kwargs:
        mode = kwargs["mode"]
    elif len(args) > 1:
        mode = args[1]
    else:
        raise NameError("'mode' argument not provided to dyad_open")
    if mode in ("r", "rb", "rt"):
        if (local_dyad_io.cons_path is not None and
                local_dyad_io.cons_path in fname.parents):
            if metadata_wrapper:
                local_dyad_io.consume_w_metadata(str(fname), metadata_wrapper)
            else:
                local_dyad_io.consume(str(fname))
    file_obj = io.open(*args, **kwargs)
    try:
        yield file_obj
    except BaseException:
        file_obj.close()
        raise
    else:
        file_obj.close()
        if mode in ("w", "wb", "wt"):
            if (local_dyad_io.prod_path is not None and
                    local_dyad_io.prod_path in fname.parents):
                local_dyad_io.produce(str(fname))
