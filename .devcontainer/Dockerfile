# Use Ubuntu 24.04 LTS as base image
FROM ubuntu:24.04

# Establish versions to use
ARG GCC_VERSION=15
ARG CLANG_VERSION=21
ARG CMAKE_VERSION=4.3.2

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive \
    VCPKG_ROOT=/opt/vcpkg \
    CMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    PATH=/usr/local/bin:/opt/vcpkg:/usr/bin:$PATH

# Install base dependencies & tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    ccache \
    cppcheck \
    curl \
    gdb \
    git \
    make \
    ninja-build \
    pkg-config \
    pre-commit \
    python3-pip \
    software-properties-common \
    tar \
    unzip \
    valgrind \
    wget \
    zip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install gcovr
RUN pip install --no-cache-dir --break-system-packages gcovr

# Install GCC, Clang, CMake and Vcpkg
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y \
    && apt-get update && apt-get install -y --no-install-recommends \
    # GCC \
    gcc-${GCC_VERSION} \
    g++-${GCC_VERSION} \
    libstdc++-${GCC_VERSION}-dev \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 100 \
    && update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-${GCC_VERSION} 100 \
    # Clang \
    && wget -O llvm.sh https://apt.llvm.org/llvm.sh \
    && chmod +x llvm.sh \
    && ./llvm.sh ${CLANG_VERSION} \
    && apt-get install -y --no-install-recommends \
    clang-${CLANG_VERSION} \
    clangd-${CLANG_VERSION} \
    clang-tidy-${CLANG_VERSION} \
    clang-format-${CLANG_VERSION} \
    lld-${CLANG_VERSION} \
    lldb-${CLANG_VERSION} \
    libc++-${CLANG_VERSION}-dev \
    libc++abi-${CLANG_VERSION}-dev \
    llvm-${CLANG_VERSION} \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/ld.lld ld.lld /usr/bin/ld.lld-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/llvm-profdata llvm-profdata /usr/bin/llvm-profdata-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/llvm-cov llvm-cov /usr/bin/llvm-cov-${CLANG_VERSION} 100 \
    && update-alternatives --install /usr/bin/run-clang-tidy run-clang-tidy /usr/bin/run-clang-tidy-${CLANG_VERSION} 100 \
    && rm llvm.sh \
    # CMake \
    && wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh \
    && chmod +x cmake-${CMAKE_VERSION}-linux-x86_64.sh \
    && ./cmake-${CMAKE_VERSION}-linux-x86_64.sh --skip-license --prefix=/usr/local \
    && rm cmake-${CMAKE_VERSION}-linux-x86_64.sh \
    # Vcpkg \
    && git clone --depth 1 https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics \
    && mkdir -p /opt/vcpkg/bincache \
    && chmod -R 777 /opt/vcpkg \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

USER ubuntu
WORKDIR /workspace

CMD ["/bin/bash"]
