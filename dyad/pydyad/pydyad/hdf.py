from pydyad.bindings import Dyad

from pathlib import Path

import h5py


class DyadFile(h5py.File):

    def __init__(self, fname,  mode, file=None, dyad_ctx=None, metadata_wrapper=None):
        # According to H5PY, the first positional argument to File.__init__ is fname
        self.fname = fname
        if not isinstance(self.fname, Path):
            self.fname = Path(fname)
        self.fname = self.fname.expanduser().resolve()
        self.m = mode
        if dyad_ctx is None:
            raise NameError("'dyad_ctx' argument not provided to pydyad.hdf.File constructor")
        self.dyad_ctx = dyad_ctx
        if self.m in ("r"):
            if (self.dyad_ctx.cons_path is not None and
                    self.dyad_ctx.cons_path in self.fname.parents):
                if metadata_wrapper:
                    self.dyad_ctx.consume_w_metadata(str(self.fname), metadata_wrapper)
                else:
                    dyad_ctx.consume(str(self.fname))
        if file:
            super().__init__(file, mode)
        else:
            super().__init__(fname, mode)

    def close(self):
        super().close()
        if self.m in ("w", "r+"):
            if (self.dyad_ctx.prod_path is not None and
                    self.dyad_ctx.prod_path in self.fname.parents):
                self.dyad_ctx.produce(str(self.fname))