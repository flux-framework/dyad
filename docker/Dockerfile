FROM ubuntu:20.04 as base 
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update 
# Install dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends git python3.10 \
    python3-venv python3-pip python3-dev openmpi-bin \
    openmpi-common libopenmpi-dev \
    gcc g++ gfortran cmake build-essential glibc-source \
    pkgconf autoconf automake libtool libtool-bin make \
    openssh-client openssh-server nmap iproute2 \
    gdb clangd clang-format vim \
    libarchive-dev ncurses-bin hwloc \
    lua5.3 liblua5.3-dev libczmq-dev \
    lua-posix-dev lz4 libzmq5 sqlite3 libsqlite3-dev
RUN python3 -m pip install jsonschema cffi ply pyyaml
RUN mkdir -p -m 0600 ~/.ssh && \
    ssh-keyscan -H github.com >> ~/.ssh/known_hosts

# Setup SSH
ENV USERNAME=root
RUN mkdir /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN passwd -d ${USERNAME}
RUN sed -i'' -e's/^#PermitRootLogin prohibit-password$/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && sed -i'' -e's/^#PasswordAuthentication yes$/PasswordAuthentication yes/' /etc/ssh/sshd_config \
    && sed -i'' -e's/^#PermitEmptyPasswords no$/PermitEmptyPasswords yes/' /etc/ssh/sshd_config \
    && sed -i'' -e's/^UsePAM yes/UsePAM no/' /etc/ssh/sshd_config

# Setup spack

ENV SHARED=/mount/shared
USER ${USERNAME}
ENV SOFTWARE=${SHARED}/software   
ENV SPACK_DIR=${SOFTWARE}/spack
ENV PROJECT_DIR ${SHARED}/code
RUN mkdir -p $SHARED
RUN chown ${USERNAME} ${SHARED}
RUN chmod +rw ${SHARED} -R
RUN mkdir -p ${SOFTWARE}
RUN mkdir -p ${SPACK_DIR}
RUN apt-get remove -y python3.12
RUN apt-get autoremove -y 
RUN echo dummy
RUN git clone https://github.com/spack/spack.git ${SPACK_DIR}
RUN sh ${SPACK_DIR}/share/spack/setup-env.sh
ENV PATH="${PATH}:${SPACK_DIR}/bin"
RUN echo dummy
COPY ./packages.yaml /root/.spack/packages.yaml
RUN spack install --sh cpp-logger@develop ucx@1.15.0
RUN apt-get install -y liblz4-dev
RUN spack install --sh flux-core@0.52.0

ENV DYAD_INSTALL_PREFIX="/mount/shared/install"
ENV DYAD_KVS_NAMESPACE="test"
ENV DYAD_DTL_MODE=UCX
ENV DYAD_PATH="/mount/ssd"
RUN mkdir -p ${DYAD_PATH}
RUN mkdir -p ${DYAD_INSTALL_PREFIX}

RUN spack view --verbose symlink ${DYAD_INSTALL_PREFIX} flux-core@0.52.0 cpp-logger@develop ucx@1.15.0

# Install DYAD
ENV PKG_CONFIG_PATH=$PKG_CONFIG_PATH:${DYAD_INSTALL_PREFIX}/lib/pkgconfig
ENV CONFIGURE_FLAGS="-DDYAD_ENABLE_UCX_DATA=ON"
ENV TEST_CONF="-DDYAD_LIBDIR_AS_LIB:BOOL=ON -DCMAKE_BUILD_TYPE=Debug -DDYAD_LOGGER=CPP_LOGGER -DDYAD_LOGGER_LEVEL=ERROR"
RUN git clone https://github.com/flux-framework/dyad.git ${PROJECT_DIR}
RUN cd ${PROJECT_DIR} && git checkout tutorial-riken-2024


RUN pip install git+https://github.com/argonne-lcf/dlio_benchmark.git

RUN pip install dlio-profiler-py
ENV DLIO_PROFILER_ENABLE=0
RUN dlio_benchmark workload=unet3d_a100 ++workload.dataset.data_folder=/root/data ++workload.workflow.generate_data=True ++workload.workflow.train=False ++workload.dataset.record_length=4096 ++workload.dataset.record_length_stdev=0 ++workload.dataset.record_length_resize=0 ++workload.reader.batch_size=1 ++workload.dataset.num_files_train=16 ++workload.reader.read_threads=1

RUN dlio_benchmark workload=unet3d_a100 ++workload.dataset.data_folder=/root/data ++workload.workflow.generate_data=False ++workload.workflow.train=True ++workload.dataset.record_length=4096 ++workload.dataset.record_length_stdev=0 ++workload.dataset.record_length_resize=0 ++workload.reader.batch_size=1 ++workload.dataset.num_files_train=16 ++workload.reader.read_threads=1

RUN mkdir -p ${PROJECT_DIR}/build \
    && cd ${PROJECT_DIR}/build \
    && cmake -DCMAKE_INSTALL_PREFIX=${DYAD_INSTALL_PREFIX} ${CONFIGURE_FLAGS} ${TEST_CONF} .. \
    && make VERBOSE=1 install -j

ENV DLIO_WORKLOAD=dyad_unet3d_demo
ENV DLIO_DATA_DIR=/mount/pfs
ENV DYAD_KVS_NAMESPACE=dyad
ENV DYAD_DTL_MODE=UCX
ENV DYAD_PATH="/mount/ssd"
ENV PATH=${PATH}:${DYAD_INSTALL_PREFIX}/bin:${DYAD_INSTALL_PREFIX}/sbin
ENV LD_LIBRARY_PATH=/usr/lib64:${DYAD_INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH}
ENV PYTHONPATH=${PROJECT_DIR}/tests/integration/dlio_benchmark:${PROJECT_DIR}/pydyad:$PYTHONPATH

RUN mkdir -p ${DYAD_PATH} ${DLIO_DATA_DIR}

RUN dlio_benchmark workload=${DLIO_WORKLOAD} --config-dir=${PROJECT_DIR}/tests/integration/dlio_benchmark/configs workload=${DLIO_WORKLOAD} ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=True ++workload.workflow.train=False

RUN dlio_benchmark workload=${DLIO_WORKLOAD} --config-dir=${PROJECT_DIR}/tests/integration/dlio_benchmark/configs workload=${DLIO_WORKLOAD} ++workload.dataset.data_folder=${DLIO_DATA_DIR} ++workload.workflow.generate_data=False ++workload.workflow.train=True

ENTRYPOINT service ssh restart && bash
