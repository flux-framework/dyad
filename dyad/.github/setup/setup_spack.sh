. ${SPACK_DIR}/share/spack/setup-env.sh
#PYTHON_VERSION=`python --version | awk {'print $2'}`
#sudo rm /usr/bin/python
#sudo ln -s `which python3.10` /usr/bin/python
mkdir -p $HOME/.spack
cat > $HOME/.spack/packages.yaml << EOF
packages:
  all:
    target: [x86_64]
    providers:
      mpi: [openmpi]
  python:
    buildable: False
    externals:
    - spec: python@3.10
      prefix: /usr
  py-jsonschema:
    buildable: False
    externals:
    - spec: "py-jsonschema@4.17.3"
      prefix: /usr
  py-cffi:
    buildable: False
    externals:
    - spec: py-cffi@1.15.1
      prefix: /usr
  py-ply:
    buildable: False
    externals:
    - spec: py-ply@3.11
      prefix: /usr
  py-pyyaml:
    buildable: False
    externals:
    - spec: py-pyyaml@6.0
      prefix: /usr
  czmq:
    buildable: False
    externals:
    - spec: czmq@4.2.1
      prefix: /usr
  sqlite:
    buildable: False
    externals:
    - spec: sqlite@3.37.2
      prefix: /usr
  libzmq:
    buildable: False
    externals:
    - spec: libzmq@4.3.4
      prefix: /usr
  lua:
    buildable: False
    externals:
    - spec: lua@5.3.6
      prefix: /usr
  lua-luaposix:
    buildable: False
    externals:
    - spec: lua-luaposix@33.4.0
      prefix: /usr
  lz4:
    buildable: False
    externals:
    - spec: lz4@1.9.3
      prefix: /usr
  ncurses:
    buildable: False
    externals:
    - spec: ncurses@6.3.2
      prefix: /usr
  pkgconf:
    buildable: False
    externals:
    - spec: pkgconf@1.8.0
      prefix: /usr
  hwloc:
    buildable: False
    externals:
    - spec: hwloc@2.7.0
      prefix: /usr
  libarchive:
    buildable: False
    externals:
    - spec: libarchive@3.6.0
      prefix: /usr
  autoconf:
    buildable: False
    externals:
    - spec: autoconf@2.69
      prefix: /usr
  automake:
    buildable: False
    externals:
    - spec: automake@1.16.1
      prefix: /usr
  libtool:
    buildable: False
    externals:
    - spec: libtool@2.4.6
      prefix: /usr
  m4:
    buildable: False
    externals:
    - spec: m4@1.4.18
      prefix: /usr
  openmpi:
    buildable: False
    externals:
    - spec: openmpi@4.0.3
      prefix: /usr
  openssl:
    buildable: False
    externals:
    - spec: openssl@1.1.1f
      prefix: /usr
  pkg-config:
    buildable: False
    externals:
    - spec: pkg-config@0.29.1
      prefix: /usr
EOF
spack external find
spack spec flux-core@${FLUX_VERSION}
if [[ $DYAD_DTL_MODE == 'UCX' ]]; then
    spack spec ucx@1.13.1
fi
spack install -j4 flux-core@${FLUX_VERSION}
if [[ $DYAD_DTL_MODE == 'UCX' ]]; then
    spack install -j4 ucx@1.13.1
fi
mkdir -p ${DYAD_INSTALL_PREFIX}
spack view --verbose symlink ${DYAD_INSTALL_PREFIX} flux-core@${FLUX_VERSION}
if [[ $DYAD_DTL_MODE == 'UCX' ]]; then
    spack view --verbose symlink ${DYAD_INSTALL_PREFIX} ucx@1.13.1
fi