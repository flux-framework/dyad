***************
Developer Guide
***************

Since DYAD is part of the `Flux Framework <https://flux-framework.org/>`_, developers
are expect to follow all rules and contribution guidelines specified in the
Collective Code Construction Contract (`C4.1 <https://github.com/flux-framework/rfc/blob/master/spec_1.rst>`_).

Below are some additional links regarding contributing, code styling, and commit
etiquette:

* `The 'Contributing' Page from the Flux Framework's ReadTheDocs <https://flux-framework.readthedocs.io/en/latest/contributing.html>`_
* `The Flux Coding Style Guide <https://github.com/flux-framework/rfc/blob/master/spec_7.rst>`_ (used for C code)
* `The black Coding Style Guide <https://black.readthedocs.io/en/stable/the_black_code_style/index.html>`_ (used for Python code)

Besides the broader Flux contribution guidelines above, we also expect C/C++ :code:`#include`
directives to be grouped. Each group should be separated by one newline.
Includes for external tools, system headers, and the C++
Standard Library should be placed in one group, and includes for internal DYAD headers
should be placed in another group. The group for external headers should be placed
before the group for internal headers.
