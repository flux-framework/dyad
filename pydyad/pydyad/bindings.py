import ctypes
import os
from pathlib import Path
import warnings

from numpy.ctypeslib import load_library


DYAD_LIB_DIR = None


class FluxHandle(ctypes.Structure):
    pass


class DyadDTLHandle(ctypes.Structure):
    pass


class DyadCtxWrapper(ctypes.Structure):
    _fields_ = [
        ("h", ctypes.POINTER(FluxHandle)),
        ("dtl_handle", ctypes.POINTER(DyadDTLHandle)),
        ("debug", ctypes.c_bool),
        ("check", ctypes.c_bool),
        ("reenter", ctypes.c_bool),
        ("initialized", ctypes.c_bool),
        ("shared_storage", ctypes.c_bool),
        ("sync_started", ctypes.c_bool),
        ("key_depth", ctypes.c_uint),
        ("key_bins", ctypes.c_uint),
        ("rank", ctypes.c_uint32),
        ("kvs_namespace", ctypes.c_char_p),
        ("prod_managed_path", ctypes.c_char_p),
        ("cons_managed_path", ctypes.c_char_p),
    ]


class Dyad:

    def __init__(self):
        self.dyad_core_lib = None
        self.ctx = None
        self.dyad_init = None
        self.dyad_init_env = None
        self.dyad_produce = None
        self.dyad_consume = None
        self.dyad_finalize = None
        dyad_core_libpath = None
        self.cons_path = None
        self.prod_path = None
        if DYAD_LIB_DIR is None:
            if "LD_LIBRARY_PATH" not in os.environ:
                raise EnvironmentError("Cannot find LD_LIBRARY_PATH in environment!")
            for p in os.environ["LD_LIBRARY_PATH"].split(":"):
                lib_path = Path(p).expanduser().resolve()
                if len(list(lib_path.glob("libdyad_core.*"))) != 0:
                    dyad_core_libpath = lib_path
                    break
            if dyad_core_libpath is None:
                raise FileNotFoundError("Cannot find libdyad_core.so!")
        else:
            dyad_core_libpath = DYAD_LIB_DIR
            if not isinstance(DYAD_LIB_DIR, Path):
                dyad_core_libpath = Path(DYAD_LIB_DIR)
            dyad_core_libpath = dyad_core_libpath.expanduser().resolve()
            if not dyad_core_libpath.is_dir():
                raise FileNotFoundError("Value of DYAD_LIB_DIR either doesn't exist or is not a directory")
            if len(list(lib_path.glob("libdyad_core.*"))) == 0:
                raise FileNotFoundError("Cannot find libdyad_core.so in value of DYAD_LIB_DIR")
        self.dyad_core_lib = load_library("libdyad_core", dyad_core_libpath)
        if self.dyad_core_lib is None:
            raise FileNotFoundError("Cannot find libdyad_core")
        self.ctx = ctypes.POINTER(DyadCtxWrapper)()
        self.dyad_init = self.dyad_core_lib.dyad_init
        self.dyad_init.argtypes = [
            ctypes.c_bool,                                   # debug
            ctypes.c_bool,                                   # check
            ctypes.c_bool,                                   # shared_storage
            ctypes.c_uint,                                   # key_depth
            ctypes.c_uint,                                   # key_bins
            ctypes.c_char_p,                                 # kvs_namespace
            ctypes.c_char_p,                                 # prod_managed_path
            ctypes.c_char_p,                                 # cons_managed_path
            ctypes.POINTER(ctypes.POINTER(DyadCtxWrapper)),  # ctx
        ]
        self.dyad_init.restype = ctypes.c_int
        self.dyad_init_env = self.dyad_core_lib.dyad_init_env
        self.dyad_init_env.argtypes = [
            ctypes.POINTER(ctypes.POINTER(DyadCtxWrapper))
        ]
        self.dyad_init_env.restype = ctypes.c_int
        self.dyad_produce = self.dyad_core_lib.dyad_produce
        self.dyad_produce.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
        ]
        self.dyad_produce.restype = ctypes.c_int
        self.dyad_consume = self.dyad_core_lib.dyad_consume
        self.dyad_consume.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
        ]
        self.dyad_consume.restype = ctypes.c_int
        self.dyad_finalize = self.dyad_core_lib.dyad_finalize
        self.dyad_finalize.argtypes = [
            ctypes.POINTER(ctypes.POINTER(DyadCtxWrapper)),
        ]
        self.dyad_finalize.restype = ctypes.c_int
        self.cons_path = None
        self.prod_path = None
        
    def init(self, debug, check, shared_storage, key_depth,
             key_bins, kvs_namespace, prod_managed_path,
             cons_managed_path):
        if self.dyad_init is None:
            warnings.warn(
                "Trying to initialize DYAD when libdyad_core.so was not found",
                RuntimeWarning
            )
            return
        res = self.dyad_init(
            ctypes.c_bool(debug),
            ctypes.c_bool(check),
            ctypes.c_bool(shared_storage),
            ctypes.c_uint(key_depth),
            ctypes.c_uint(key_bins),
            kvs_namespace.encode() if kvs_namespace is not None else None,
            prod_managed_path.encode() if prod_managed_path is not None else None,
            cons_managed_path.encode() if cons_managed_path is not None else None,
            ctypes.byref(self.ctx),
        )
        if int(res) != 0:
            raise RuntimeError("Could not initialize DYAD!")
        if self.ctx.contents.prod_managed_path is None:
            self.prod_path = None
        else:
            self.prod_path = Path(self.ctx.contents.prod_managed_path.decode("utf-8")).expanduser().resolve()
        if self.ctx.contents.cons_managed_path is None:
            self.cons_path = None
        else:
            self.cons_path = Path(self.ctx.contents.cons_managed_path.decode("utf-8")).expanduser().resolve()
        
    def init_env(self):
        if self.dyad_init_env is None:
            warnings.warn(
                "Trying to initialize DYAD when libdyad_core.so was not found",
                RuntimeWarning
            )
            return
        res = self.dyad_init_env(
            ctypes.byref(self.ctx)
        )
        if int(res) != 0:
            raise RuntimeError("Could not initialize DYAD with environment variables")
        if self.ctx.contents.prod_managed_path is None:
            self.prod_path = None
        else:
            self.prod_path = Path(self.ctx.contents.prod_managed_path.decode("utf-8")).expanduser().resolve()
        if self.ctx.contents.cons_managed_path is None:
            self.cons_path = None
        else:
            self.cons_path = Path(self.ctx.contents.cons_managed_path.decode("utf-8")).expanduser().resolve()
        
    def __del__(self):
        self.finalize()
    
    def produce(self, fname):
        if self.dyad_produce is None:
            warnings.warn(
                "Trying to produce with DYAD when libdyad_core.so was not found",
                RuntimeWarning
            )
            return
        res = self.dyad_produce(
            self.ctx,
            fname.encode(),
        )
        if int(res) != 0:
            raise RuntimeError("Cannot produce data with DYAD!")
    
    def consume(self, fname):
        if self.dyad_consume is None:
            warnings.warn(
                "Trying to consunme with DYAD when libdyad_core.so was not found",
                RuntimeWarning
            )
            return
        res = self.dyad_consume(
            self.ctx,
            fname.encode(),
        )
        if int(res) != 0:
            raise RuntimeError("Cannot consume data with DYAD!")
    
    def finalize(self):
        if self.dyad_finalize is None:
            warnings.warn(
                "Trying to finalize DYAD when libdyad_core.so was not found",
                RuntimeWarning
            )
            return
        res = self.dyad_finalize(
            ctypes.byref(self.ctx)
        )
        if int(res) != 0:
            raise RuntimeError("Cannot finalize DYAD!")
