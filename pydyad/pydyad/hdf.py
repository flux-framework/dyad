from pydyad.bindings import Dyad

from pathlib import Path

import h5py


class DyadFile(h5py.File):

    def __init__(self, *args, **kwargs, dyad_ctx=None, metadata_wrapper=None):
        # According to H5PY, the first positional argument to File.__init__ is fname
        self.fname = args[0]
        if not isinstance(self.fname, Path):
            self.fname = Path(self.fname)
        self.fname = self.fname.expanduser().resolve()
        self.mode = None
        if "mode" in kwargs:
            self.mode = kwargs["mode"]
        elif len(args) > 1:
            self.mode = args[1]
        else:
            raise NameError("'mode' argument not provided to pydyad.hdf.File constructor")
        if dyad_ctx is None:
            raise NameError("'dyad_ctx' argument not provided to pydyad.hdf.File constructor")
        self.dyad_ctx = dyad_ctx
        if self.mode in ("r", "rb", "rt"):
            if (self.dyad_ctx.cons_path is not None and
                    self.dyad_ctx.cons_path in self.fname.parents):
                if metadata_wrapper:
                    self.dyad_ctx.consume_w_metadata(str(self.fname), meatadata_wrapper)
                else:
                    dyad_ctx.consume(str(self.fname))
        super().__init__(*args, **kwargs)


    def close(self):
        super().close()
        if self.mode in ("w", "wb", "wt"):
            if (self.dyad_ctx.prod_path is not None and
                    self.dyad_ctx.prod_path in self.fname.parents):
                self.dyad_ctx.produce(str(self.fname))