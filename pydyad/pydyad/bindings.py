import ctypes
from ctypes.util import find_library
import enum
from pathlib import Path
import warnings
try:
    from dftracer.python import dftracer, dft_fn
    dft_log = dft_fn("DYAD_PY")
except ImportError:
    class _NoopLog:
        def log(self, fn):
            return fn
    class _NoopDftracer:
        def initialize_log(self, **kwargs):
            return None
    dftracer = _NoopDftracer()
    dft_log = _NoopLog()

DYAD_LIB_DIR = None


class FluxHandle(ctypes.Structure):
    pass


class DyadDTLHandle(ctypes.Structure):
    pass


class DyadCachePolicyHandle(ctypes.Structure):
    pass


class DyadCtxWrapper(ctypes.Structure):
    _fields_ = [
        ("h", ctypes.POINTER(FluxHandle)),
        ("dtl_handle", ctypes.POINTER(DyadDTLHandle)),
        ("fname", ctypes.c_char_p),
        ("use_fs_locks", ctypes.c_bool),
        ("prod_real_path", ctypes.c_char_p),
        ("cons_real_path", ctypes.c_char_p),
        ("prod_managed_len", ctypes.c_uint32),
        ("cons_managed_len", ctypes.c_uint32),
        ("prod_real_len", ctypes.c_uint32),
        ("cons_real_len", ctypes.c_uint32),
        ("prod_managed_hash", ctypes.c_uint32),
        ("cons_managed_hash", ctypes.c_uint32),
        ("prod_real_hash", ctypes.c_uint32),
        ("cons_real_hash", ctypes.c_uint32),
        ("delim_len", ctypes.c_uint32),
        ("debug", ctypes.c_bool),
        ("check", ctypes.c_bool),
        ("reenter", ctypes.c_bool),
        ("initialized", ctypes.c_bool),
        ("shared_storage", ctypes.c_bool),
        ("async_publish", ctypes.c_bool),
        ("fsync_write", ctypes.c_bool),
        ("key_depth", ctypes.c_uint),
        ("key_bins", ctypes.c_uint),
        ("rank", ctypes.c_uint32),
        ("service_mux", ctypes.c_uint32),
        ("node_idx", ctypes.c_uint32),
        ("pid", ctypes.c_int32),
        ("kvs_namespace", ctypes.c_char_p),
        ("prod_managed_path", ctypes.c_char_p),
        ("cons_managed_path", ctypes.c_char_p),
        ("relative_to_managed_path", ctypes.c_bool),
        ("cache_policy", ctypes.POINTER(DyadCachePolicyHandle)),
        ("cache_policy_mode", ctypes.c_int),
        ("cache_capacity_bytes", ctypes.c_uint64),
        ("cache_low_watermark_frac", ctypes.c_double),
        ("cache_grace_period_sec", ctypes.c_uint),
        ("origin_path", ctypes.c_char_p),
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
                raise AttributeError(
                    "{} is not an attribute of DYAD's metadata".format(attr_name)
                )
        raise AttributeError("Underlying metadata object has already been freed")

    def __del__(self):
        if self.mdata is not None:
            self.dyad_bindings_obj.free_metadata(self.mdata)
            self.mdata = None
            self.dyad_bindings_obj = None


class DTLMode(enum.Enum):
    DYAD_DTL_UCX = "UCX"
    DYAD_DTL_MARGO = "MARGO"
    DYAD_DTL_FLUX_RPC = "FLUX_RPC"

    def __str__(self):
        return self.value


class DTLCommMode(enum.IntEnum):
    DYAD_COMM_NONE = 0
    DYAD_COMM_RECV = 1
    DYAD_COMM_SEND = 2
    DYAD_COMM_END = 3


class CachePolicyMode(enum.Enum):
    DYAD_CACHE_NONE = "NONE"
    DYAD_CACHE_LRU = "LRU"
    DYAD_CACHE_FIFO = "FIFO"

    def __str__(self):
        return self.value


class Dyad:
    def __init__(self):
        self.initialized = False
        self.dyad_client_lib = None
        self.dyad_ctx_lib = None
        self.ctx = None
        self.dyad_init = None
        self.dyad_init_env = None
        self.dyad_produce = None
        self.dyad_consume = None
        self.dyad_consume_w_metadata = None
        self.dyad_finalize = None
        dyad_client_lib_file = None
        dyad_ctx_lib_file = None
        self.cons_path = None
        self.prod_path = None

        dyad_client_lib_file = find_library("dyad_client")
        if dyad_client_lib_file is None:
            raise FileNotFoundError("Cannot find libdyad_client.so using 'ctypes'")
        self.dyad_client_lib = ctypes.CDLL(dyad_client_lib_file)
        if self.dyad_client_lib is None:
            raise FileNotFoundError("Cannot find libdyad_client")

        dyad_ctx_lib_file = find_library("dyad_ctx")
        if dyad_ctx_lib_file is None:
            raise FileNotFoundError("Cannot find libdyad_ctx.so using 'ctypes'")
        self.dyad_ctx_lib = ctypes.CDLL(dyad_ctx_lib_file)
        if self.dyad_ctx_lib is None:
            raise FileNotFoundError("Cannot find libdyad_ctx")

        self.dyad_ctx_init = self.dyad_ctx_lib.dyad_ctx_init
        self.dyad_ctx_init.argtypes = [
            ctypes.c_int,  # dtl_comm_mode
            ctypes.c_void_p,  # flux_handle
        ]
        self.dyad_ctx_init.restype = None

        self.dyad_ctx_get = self.dyad_ctx_lib.dyad_ctx_get
        self.dyad_ctx_get.argtypes = []
        self.dyad_ctx_get.restype = ctypes.POINTER(DyadCtxWrapper)

        self.dyad_init = self.dyad_ctx_lib.dyad_init
        self.dyad_init.argtypes = [
            ctypes.c_bool,  # debug
            ctypes.c_bool,  # check
            ctypes.c_bool,  # shared_storage
            ctypes.c_bool,  # reinit
            ctypes.c_bool,  # async_publish
            ctypes.c_bool,  # fsync_write
            ctypes.c_uint,  # key_depth
            ctypes.c_uint,  # key_bins
            ctypes.c_uint,  # service_mux
            ctypes.c_char_p,  # kvs_namespace
            ctypes.c_char_p,  # prod_managed_path
            ctypes.c_char_p,  # cons_managed_path
            ctypes.c_bool,  # relative_to_managed_path
            ctypes.c_char_p,  # dtl_mode
            ctypes.c_int,  # dtl_comm_mode
            ctypes.c_void_p,  # flux_handle
            ctypes.c_uint64,  # cache_capacity_bytes
            ctypes.c_char_p,  # cache_policy
            ctypes.c_double,  # cache_low_watermark
            ctypes.c_uint,  # cache_grace_period_sec
            ctypes.c_char_p,  # origin_path
        ]
        self.dyad_init.restype = ctypes.c_int

        self.dyad_init_env = self.dyad_ctx_lib.dyad_init_env
        self.dyad_init_env.argtypes = [
            ctypes.c_int,  # dtl_comm_mode
            ctypes.c_void_p,  # flux_handle
        ]
        self.dyad_init_env.restype = ctypes.c_int

        self.dyad_produce = self.dyad_client_lib.dyad_produce
        self.dyad_produce.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
        ]
        self.dyad_produce.restype = ctypes.c_int

        self.dyad_get_metadata = self.dyad_client_lib.dyad_get_metadata
        self.dyad_get_metadata.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
            ctypes.c_bool,
            ctypes.POINTER(ctypes.POINTER(DyadMetadataWrapper)),
        ]
        self.dyad_get_metadata.restype = ctypes.c_int

        self.dyad_free_metadata = self.dyad_client_lib.dyad_free_metadata
        self.dyad_free_metadata.argtypes = [
            ctypes.POINTER(ctypes.POINTER(DyadMetadataWrapper))
        ]
        self.dyad_free_metadata.restype = ctypes.c_int

        self.dyad_consume = self.dyad_client_lib.dyad_consume
        self.dyad_consume.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
        ]
        self.dyad_consume.restype = ctypes.c_int

        self.dyad_consume_w_metadata = self.dyad_client_lib.dyad_consume_w_metadata
        self.dyad_consume_w_metadata.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
            ctypes.POINTER(DyadMetadataWrapper),
        ]
        self.dyad_consume_w_metadata.restype = ctypes.c_int

        self.dyad_consume_range = self.dyad_client_lib.dyad_consume_range
        self.dyad_consume_range.argtypes = [
            ctypes.POINTER(DyadCtxWrapper),
            ctypes.c_char_p,
            ctypes.c_size_t,  # offset
            ctypes.c_size_t,  # length
            ctypes.POINTER(ctypes.c_void_p),  # data (out)
            ctypes.POINTER(ctypes.c_size_t),  # data_len (out)
        ]
        self.dyad_consume_range.restype = ctypes.c_int

        self.dyad_finalize = self.dyad_ctx_lib.dyad_finalize
        self.dyad_finalize.argtypes = []
        self.dyad_finalize.restype = ctypes.c_int

        self.cons_path = None
        self.prod_path = None
        self.log_inst = None

    def init(
        self,
        debug=False,
        check=False,
        shared_storage=False,
        reinit=False,
        async_publish=False,
        fsync_write=False,
        key_depth=3,
        key_bins=1024,
        service_mux=1,
        kvs_namespace=None,
        prod_managed_path=None,
        cons_managed_path=None,
        relative_to_managed_path=False,
        dtl_mode=None,
        dtl_comm_mode=DTLCommMode.DYAD_COMM_RECV,
        flux_handle=None,
        cache_capacity_bytes=0,
        cache_policy=None,
        cache_low_watermark=0.8,
        cache_grace_period_sec=5,
        origin_path=None,
    ):
        self.log_inst = dftracer.initialize_log(
            logfile=None, data_dir=None, process_id=-1
        )
        if self.dyad_init is None:
            warnings.warn(
                "Trying to initialize DYAD when libdyad_ctx.so was not found",
                RuntimeWarning,
            )
            return
        res = self.dyad_init(
            ctypes.c_bool(debug),
            ctypes.c_bool(check),
            ctypes.c_bool(shared_storage),
            ctypes.c_bool(reinit),
            ctypes.c_bool(async_publish),
            ctypes.c_bool(fsync_write),
            ctypes.c_uint(key_depth),
            ctypes.c_uint(key_bins),
            ctypes.c_uint(service_mux),
            kvs_namespace.encode() if kvs_namespace is not None else None,
            prod_managed_path.encode() if prod_managed_path is not None else None,
            cons_managed_path.encode() if cons_managed_path is not None else None,
            ctypes.c_bool(relative_to_managed_path),
            str(dtl_mode).encode() if dtl_mode is not None else None,
            ctypes.c_int(dtl_comm_mode),
            ctypes.c_void_p(flux_handle),
            ctypes.c_uint64(cache_capacity_bytes),
            str(cache_policy).encode() if cache_policy is not None else None,
            ctypes.c_double(cache_low_watermark),
            ctypes.c_uint(cache_grace_period_sec),
            origin_path.encode() if origin_path is not None else None,
        )
        self.ctx = self.dyad_ctx_get()

        if int(res) != 0:
            raise RuntimeError("Could not initialize DYAD!")
        if self.ctx.contents.prod_managed_path is None:
            self.prod_path = None
        else:
            self.prod_path = (
                Path(self.ctx.contents.prod_managed_path.decode("utf-8"))
                .expanduser()
                .resolve()
            )
        if self.ctx.contents.cons_managed_path is None:
            self.cons_path = None
        else:
            self.cons_path = (
                Path(self.ctx.contents.cons_managed_path.decode("utf-8"))
                .expanduser()
                .resolve()
            )
        self.initialized = True

    def init_env(self, dtl_comm_mode=DTLCommMode.DYAD_COMM_RECV, flux_handle=None):
        self.log_inst = dftracer.initialize_log(
            logfile=None, data_dir=None, process_id=-1
        )
        if self.dyad_init_env is None:
            warnings.warn(
                "Trying to initialize DYAD when libdyad_ctx.so was not found",
                RuntimeWarning,
            )
            return
        res = self.dyad_init_env(
            ctypes.c_int(dtl_comm_mode), ctypes.c_void_p(flux_handle)
        )
        self.ctx = self.dyad_ctx_get()
        if int(res) != 0:
            raise RuntimeError("Could not initialize DYAD with environment variables")
        if self.ctx.contents.prod_managed_path is None:
            self.prod_path = None
        else:
            self.prod_path = (
                Path(self.ctx.contents.prod_managed_path.decode("utf-8"))
                .expanduser()
                .resolve()
            )
        if self.ctx.contents.cons_managed_path is None:
            self.cons_path = None
        else:
            self.cons_path = (
                Path(self.ctx.contents.cons_managed_path.decode("utf-8"))
                .expanduser()
                .resolve()
            )
        self.initialized = True

    def __del__(self):
        self.finalize()

    @dft_log.log
    def produce(self, fname):
        if self.dyad_produce is None:
            warnings.warn(
                "Trying to produce with DYAD when libdyad_client.so was not found",
                RuntimeWarning,
            )
            return
        res = self.dyad_produce(
            self.ctx,
            fname.encode(),
        )
        if int(res) != 0:
            raise RuntimeError("Cannot produce data with DYAD!")

    @dft_log.log
    def get_metadata(self, fname, should_wait=False, raw=False):
        if self.dyad_get_metadata is None:
            warnings.warn(
                "Trying to get metadata for file with DYAD when libdyad_client.so was not found",
                RuntimeWarning,
            )
            return None
        mdata = ctypes.POINTER(DyadMetadataWrapper)()
        res = self.dyad_get_metadata(
            self.ctx, fname.encode(), should_wait, ctypes.byref(mdata)
        )
        if int(res) != 0:
            return None
        if not raw:
            return DyadMetadata(mdata, self)
        return mdata

    @dft_log.log
    def free_metadata(self, metadata_wrapper):
        if self.dyad_free_metadata is None:
            warnings.warn(
                "Trying to free DYAD metadata when libdyad_client.so was not found",
                RuntimeWarning,
            )
            return
        res = self.dyad_free_metadata(ctypes.byref(metadata_wrapper))
        if int(res) != 0:
            raise RuntimeError("Could not free DYAD metadata")

    @dft_log.log
    def consume(self, fname):
        if self.dyad_consume is None:
            warnings.warn(
                "Trying to consunme with DYAD when libdyad_client.so was not found",
                RuntimeWarning,
            )
            return
        res = self.dyad_consume(
            self.ctx,
            fname.encode(),
        )
        if int(res) != 0:
            raise RuntimeError("Cannot consume data with DYAD!")

    @dft_log.log
    def consume_w_metadata(self, fname, metadata_wrapper):
        if self.dyad_consume is None:
            warnings.warn(
                "Trying to consunme with metadata  with DYAD when libdyad_client.so was not found",
                RuntimeWarning,
            )
            return
        res = self.dyad_consume_w_metadata(self.ctx, fname.encode(), metadata_wrapper)
        if int(res) != 0:
            raise RuntimeError("Cannot consume data with metadata with DYAD!")

    @dft_log.log
    def consume_range(self, fname, offset, length):
        """Fetch a byte range [offset, offset + length) of fname without
        materializing a local copy of the whole file. Only supported under
        DYAD_DTL_FLUX_RPC and DYAD_DTL_MARGO. Returns the requested bytes
        directly as a Python `bytes` object.
        """
        if self.dyad_consume_range is None:
            warnings.warn(
                "Trying to consume a byte range with DYAD when libdyad_client.so was not found",
                RuntimeWarning,
            )
            return None
        data_ptr = ctypes.c_void_p()
        data_len = ctypes.c_size_t()
        res = self.dyad_consume_range(
            self.ctx,
            fname.encode(),
            ctypes.c_size_t(offset),
            ctypes.c_size_t(length),
            ctypes.byref(data_ptr),
            ctypes.byref(data_len),
        )
        if int(res) != 0:
            raise RuntimeError("Cannot consume byte range with DYAD!")
        data = ctypes.string_at(data_ptr, data_len.value)
        # dyad_consume_range() returns a plain malloc()-family buffer (either
        # a direct malloc() for the local-pread path, or the DTL's
        # get_buffer() allocation for the remote-RPC path -- both
        # free()-compatible for FLUX_RPC/MARGO). ctypes.string_at() already
        # copied the bytes into Python-owned memory above, so free the C
        # buffer now via libc directly (DYAD has no dedicated free function
        # for this, unlike dyad_free_metadata()).
        ctypes.CDLL(None).free(data_ptr)
        return data

    @dft_log.log
    def finalize(self):
        if not self.initialized:
            return
        if self.log_inst:
            self.log_inst.finalize()
        if self.dyad_finalize is None:
            warnings.warn(
                "Trying to finalize DYAD when libdyad_ctx.so was not found",
                RuntimeWarning,
            )
            return
        res = self.dyad_finalize()
        # Without this, `initialized` stays True forever, so the guard at
        # the top of this method never actually prevents a second call --
        # e.g. an explicit finalize() followed by __del__() at interpreter
        # exit -- from calling dyad_finalize() twice on already-freed C-side
        # state (a double-finalize/use-after-free).
        self.initialized = False
        if int(res) != 0:
            raise RuntimeError("Cannot finalize DYAD!")
