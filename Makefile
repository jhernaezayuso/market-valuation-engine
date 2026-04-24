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

.PHONY: docker-build docker-run lint configure build test tests clang-tidy tidy-diff clean clear

docker-build:
	docker build -t market-valuation-engine-env -f .devcontainer/Dockerfile .

docker-run:
	docker run --rm -it -v $(shell pwd):/workspace market-valuation-engine-env

lint:
	pre-commit run --all-files

configure:
	cmake --preset $(PRESET) $(CMAKE_ARGS)

build:
	cmake --build --preset $(PRESET)

test tests:
	ctest --test-dir build/$(PRESET) --output-on-failure

clang-tidy:
	run-clang-tidy -p build/$(PRESET) -header-filter='.*' -quiet

tidy-diff:
	git diff $(BASE_BRANCH)...HEAD --name-only | grep -E "\.(cpp|hpp)$$" | xargs -r clang-tidy -p build/$(PRESET) -header-filter='.*' -quiet

clean clear:
	rm -rf build/
