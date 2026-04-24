# Market Valuation Engine

<div align="center">

![CI](https://github.com/jhernaezayuso/market-valuation-engine/actions/workflows/ci.yml/badge.svg)
![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg)

</div>

A high-performance financial risk engine developed in Modern C++.
Evaluates Credit Value Adjustment (CVA) on Interest Rate Swaps using Monte Carlo simulations, SIMD vectorization, and multi-threading.

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

## Testing

The engine includes a comprehensive test suite utilizing Google Test.

Configure and build the test suite with coverage instrumentation:

```shell
make configure COMPILER=clang BUILD_TYPE=debug BUILD_TESTS=ON
make build COMPILER=clang BUILD_TYPE=debug
```

Run the tests:

```shell
make test COMPILER=clang BUILD_TYPE=debug
```

---

*Developed by Jorge Hernáez Ayuso*
