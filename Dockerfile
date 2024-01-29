FROM almalinux:8

RUN dnf -v -y update \
&&  dnf -v -y install \
    gcc-toolset-11 \
    llvm-toolset \
    git-core \
    python3-devel \
    cmake \
    gcc-toolset-11-libasan-devel \
    gcc-toolset-11-liblsan-devel \
    gcc-toolset-11-libtsan-devel \
    gcc-toolset-11-libubsan-devel \
    clang-tools-extra \
    redis \
    perf \
    valgrind \
    systemtap-sdt-devel \
&&  dnf -v -y debuginfo-install \
    libgcc \
    libstdc++ \
&&  dnf -v -y clean all \
&&  pip3 install --upgrade 'conan<2'

ENV PATH=/opt/rh/gcc-toolset-11/root/bin:/usr/share/Modules/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV LD_LIBRARY_PATH=/opt/rh/gcc-toolset-11/root/lib
