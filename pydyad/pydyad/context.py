from pydyad.bindings import Dyad

from contextlib import contextmanager
import io
from pathlib import Path

# from dlio_profiler.logger import fn_interceptor
# dlio_log = fn_interceptor("DYAD_PY")
DYAD_IO = None


# The design of dyad_open is based on the PEP for Python's 'with' syntax:
# https://peps.python.org/pep-0343/
@contextmanager
# @dlio_log.log
def dyad_open(
    *args, dyad_ctx=None, metadata_wrapper=None, register_dyad_ctx=False, **kwargs
):
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
        if (
            local_dyad_io.cons_path is not None
            and local_dyad_io.cons_path in fname.parents
        ):
            if metadata_wrapper:
                local_dyad_io.consume_w_metadata(str(fname), metadata_wrapper)
            else:
                local_dyad_io.consume(str(fname))
    file_obj = io.open(*args, **kwargs)
    try:
        yield file_obj
    finally:
        file_obj.close()
        if mode in ("w", "wb", "wt"):
            if (
                local_dyad_io.prod_path is not None
                and local_dyad_io.prod_path in fname.parents
            ):
                local_dyad_io.produce(str(fname))
