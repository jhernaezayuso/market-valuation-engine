# Market Valuation Engine

<div align="center">

![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg)

</div>

A high-performance financial risk engine developed in Modern C++.

---

## Prerequisites

To build and run this project, you need the following tools:
* **Compilers:** GCC 15+ or Clang 21+
* **Build System:** CMake 4.3.2+ and Ninja
* **Dependency Manager:** Vcpkg

### Docker Environment
We provide a pre-configured **Docker** environment and a **DevContainer** with all the requirements.
Build the image and run the container:

```shell
make docker-build
make docker-run
```

---

## Build & Run

To compile the application with maximum performance optimizations (Release mode):

```shell
make configure COMPILER=gcc BUILD_TYPE=release
make build COMPILER=gcc BUILD_TYPE=release
```

---

*Developed by Jorge Hernáez Ayuso*
