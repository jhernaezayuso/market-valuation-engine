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

Run the CVA analysis CLI tool:

```shell
./build/gcc-release/apps/swap_cva
```

### Numerical configuration

Three options select how the engine is compiled.

| Option | Default | Values                                                | Effect on the result                                                          |
|---|---|-------------------------------------------------------|-------------------------------------------------------------------------------|
| `ARCH` | `native` | `native`, a named level such as `x86-64-v3`, or `off` | None                                                                          |
| `SIMD_WIDTH` | `4` | A power of two from `1` to `32`                       | Paths are identical but the reduction to their size moves their last digits |
| `FP_CONTRACT` | `off` | `off`, `fast`, or `default`                           | `fast` makes the two compilers disagree in the last digits                    |

```shell
make configure COMPILER=gcc BUILD_TYPE=release SIMD_WIDTH=8 FP_CONTRACT=fast
```

---

## Testing & Coverage

The engine includes a comprehensive test suite utilizing Google Test.

Configure and build the test suite with coverage instrumentation:

```shell
make configure COMPILER=clang BUILD_TYPE=debug BUILD_TESTS=ON ENABLE_COVERAGE=ON
make build COMPILER=clang BUILD_TYPE=debug
```

Run the tests:

```shell
make test COMPILER=clang BUILD_TYPE=debug
```

Generate the coverage report (output will be located in `build/clang-debug/coverage_report/`):

```shell
make coverage_report COMPILER=clang BUILD_TYPE=debug
```

---

## Benchmarks

Each stage of the CVA pipeline can be benchmarked. To do so, benchmarks must be built in Release mode.

```shell
make configure COMPILER=clang BUILD_TYPE=release BUILD_BENCHMARKS=ON
make build COMPILER=clang BUILD_TYPE=release
```

Run the suite, optionally filtering cases:

```shell
make benchmark COMPILER=clang BUILD_TYPE=release
make benchmark COMPILER=clang BUILD_TYPE=release BENCHMARK_ARGS="--benchmark_filter=layout"
```

Write a reproducible report averaged over several repetitions (output in `build/clang-release/reports/`):

```shell
make benchmark_report COMPILER=clang BUILD_TYPE=release
```

---

*Developed by Jorge Hernáez Ayuso*
