# Default configuration
COMPILER ?= gcc
BUILD_TYPE ?= release
BASE_BRANCH ?= master

COMPILER_LOWER_CASE := $(shell echo $(COMPILER) | tr A-Z a-z)
BUILD_TYPE_LOWER_CASE := $(shell echo $(BUILD_TYPE) | tr A-Z a-z)
PRESET := $(COMPILER_LOWER_CASE)-$(BUILD_TYPE_LOWER_CASE)

# Build options
CMAKE_ARGS :=
ifdef BUILD_TESTS
    CMAKE_ARGS += -DXVA_BUILD_TESTS=$(BUILD_TESTS)
endif
ifdef ENABLE_COVERAGE
    CMAKE_ARGS += -DXVA_ENABLE_COVERAGE=$(ENABLE_COVERAGE)
endif
ifdef BUILD_BENCHMARKS
    CMAKE_ARGS += -DXVA_BUILD_BENCHMARKS=$(BUILD_BENCHMARKS)
endif
ifdef SIMD_WIDTH
    CMAKE_ARGS += -DXVA_SIMD_WIDTH=$(SIMD_WIDTH)
endif
ifdef ARCH
    CMAKE_ARGS += -DXVA_ARCH=$(ARCH)
endif
ifdef FP_CONTRACT
    CMAKE_ARGS += -DXVA_FP_CONTRACT=$(FP_CONTRACT)
endif
ifdef BENCHMARK_REPETITIONS
    CMAKE_ARGS += -DXVA_BENCHMARK_REPETITIONS=$(BENCHMARK_REPETITIONS)
endif
ifdef BENCHMARK_MIN_TIME
    CMAKE_ARGS += -DXVA_BENCHMARK_MIN_TIME=$(BENCHMARK_MIN_TIME)
endif

# Benchmark options
BENCHMARK_ARGS :=

.PHONY: docker-build docker-run lint configure build test tests coverage_report clang-tidy tidy-diff \
        benchmark benchmarks benchmark_report clean clear

docker-build:
	docker build -t market-valuation-engine-env -f .devcontainer/Dockerfile .

docker-run:
	docker run --rm -it --security-opt seccomp=unconfined -v $(shell pwd):/workspace market-valuation-engine-env

lint:
	pre-commit run --all-files

configure:
	cmake --preset $(PRESET) $(CMAKE_ARGS)

build:
	cmake --build --preset $(PRESET)

test tests:
	ctest --test-dir build/$(PRESET) --output-on-failure

coverage_report:
	cmake --build --preset $(PRESET) --target coverage_report

clang-tidy:
	run-clang-tidy -p build/$(PRESET) -header-filter='.*' -quiet

tidy-diff:
	git diff $(BASE_BRANCH)...HEAD --name-only | grep -E "\.(cpp|hpp)$$" | xargs -r clang-tidy -p build/$(PRESET) -header-filter='.*' -quiet

benchmark benchmarks:
	./build/$(PRESET)/benchmarks/xva_benchmarks $(BENCHMARK_ARGS)

benchmark_report:
	cmake --build --preset $(PRESET) --target benchmark_report

clean clear:
	rm -rf build/
