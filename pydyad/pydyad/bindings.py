import ctypes
from ctypes.util import find_library
import enum
from pathlib import Path
import warnings


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
        ("service_mux", ctypes.c_uint32),
        ("node_idx", ctypes.c_uint32),
        ("pid", ctypes.c_int32),
        ("kvs_namespace", ctypes.c_char_p),
        ("prod_managed_path", ctypes.c_char_p),
        ("cons_managed_path", ctypes.c_char_p),
    ]


class DyadMetadataWrapper(ctypes.Structure):
    _fields_ = [
        ("fpath", ctypes.c_char_p),
        ("owner_rank", ctypes.c_uint32),
    ]


class DyadMetadata:

    def __init__(self, metadata_wrapper, dyad_obj):
        self.mdata = metadata_wrapper
        self.dyad_bindings_obj = dyad_obj

    def __getattr__(self, attr_name):
        if self.mdata is not None: 
            try:
                return getattr(self.mdata.contents, attr_name)
            except AttributeError:
                raise AttributeError("{} is not an attribute of DYAD's metadata".format(attr_name))
        raise AttributeError("Underlying metadata object has already been freed")

    def __del__(self):
        if self.mdata is not None:
            self.dyad_bindings_obj.free_metadata(self.mdata)
            self.mdata = None
            self.dyad_bindings_obj = None


class DTLMode(enum.IntEnum):
    DYAD_DTL_UCX = 0
    DYAD_DTL_FLUX_RPC = 1


class Dyad:

    def __init__(self):
        self.initialized = False
        self.dyad_core_lib = None
        self.ctx = None
        self.dyad_init = None
        self.dyad_init_env = None
        self.dyad_produce = None
        self.dyad_consume = None
        self.dyad_finalize = None
        dyad_core_lib_file = None
        self.cons_path = None
        self.prod_path = None
        dyad_core_lib_file = find_library("dyad_core")
        if dyad_core_lib_file is None:
            raise FileNotFoundError("Cannot find libdyad_core.so using 'ctypes'")
        self.dyad_core_lib = ctypes.CDLL(dyad_core_lib_file)
        if self.dyad_core_lib is None:
            raise FileNotFoundError("Cannot find libdyad_core")
        self.ctx = ctypes.POINTER(DyadCtxWrapper)()
        self.dyad_init = self.dyad_core_lib.dyad_init
        self.dyad_init.argtypes = [
            ctypes.c_bool,                                   # debug
            ctypes.c_bool,                                   # check
            ctypes.c_bool,                                   # shared_storage
            ctypes.c_bool,                                   # reinit
            ctypes.c_uint,                                   # key_depth
            ctypes.c_uint,                                   # key_bins
            ctypes.c_uint,                                   # service_mux
            ctypes.c_uint,                                   # node_idx
            ctypes.c_int,                                    # pid
            ctypes.c_char_p,                                 # kvs_namespace
            ctypes.c_char_p,                                 # prod_managed_path
            ctypes.c_char_p,                                 # cons_managed_path
            ctypes.c_int,                                    # dtl_mode
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
        self.dyad_get_metadata = self.dyad_core_lib.dyad_get_metadata
        self.dyad_get_metadata.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
            ctypes.c_bool,
            ctypes.POINTER(ctypes.POINTER(DyadMetadataWrapper)),
        ]
        self.dyad_get_metadata.restype = ctypes.c_int
        self.dyad_free_metadata = self.dyad_core_lib.dyad_free_metadata
        self.dyad_free_metadata.argtypes = [
            ctypes.POINTER(ctypes.POINTER(DyadMetadataWrapper))
        ]
        self.dyad_free_metadata.restype = ctypes.c_int
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
        
    def init(
        self,
        debug=False,
        check=False,
        shared_storage=False,
        reinit=False,
        key_depth=3,
        key_bins=1024,
        service_mux=1,
        kvs_namespace=None,
        prod_managed_path=None,
        cons_managed_path=None,
        dtl_mode=DTLMode.DYAD_DTL_FLUX_RPC,
    ):
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
            ctypes.c_bool(reinit),
            ctypes.c_uint(key_depth),
            ctypes.c_uint(key_bins),
            ctypes.c_uint(service_mux),
            kvs_namespace.encode() if kvs_namespace is not None else None,
            prod_managed_path.encode() if prod_managed_path is not None else None,
            cons_managed_path.encode() if cons_managed_path is not None else None,
            ctypes.c_int(dtl_mode),
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
        self.initialized = True
        
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
        
    def get_metadata(self, fname, should_wait=False, raw=False):
        if self.dyad_get_metadata is None:
            warnings.warn(
                "Trying to get metadata for file with DYAD when libdyad_core.so was not found",
                RuntimeWarning
            )
            return None
        mdata = ctypes.POINTER(DyadMetadataWrapper)()
        res = self.dyad_get_metadata(
            self.ctx,
            fname.encode(),
            should_wait,
            ctypes.byref(mdata)
        )
        if int(res) != 0:
            return None
        if not raw:
            return DyadMetadata(mdata, self)
        return mdata
        
    def free_metadata(self, metadata_wrapper):
        if self.dyad_free_metadata is None:
            warnings.warn("Trying to free DYAD metadata when libdyad_core.so was not found", RuntimeWarning)
            return
        res = self.dyad_free_metadata(
            ctypes.byref(metadata_wrapper)
        )
        if int(res) != 0:
            raise RuntimeError("Could not free DYAD metadata")
    
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
        if not self.initialized:
            return
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
