# Default configuration
COMPILER ?= gcc
BUILD_TYPE ?= release

COMPILER_LOWER_CASE := $(shell echo $(COMPILER) | tr A-Z a-z)
BUILD_TYPE_LOWER_CASE := $(shell echo $(BUILD_TYPE) | tr A-Z a-z)
PRESET := $(COMPILER_LOWER_CASE)-$(BUILD_TYPE_LOWER_CASE)

.PHONY: docker-build docker-run lint configure build clean clear

docker-build:
	docker build -t market-valuation-engine-env -f .devcontainer/Dockerfile .

docker-run:
	docker run --rm -it -v $(shell pwd):/workspace market-valuation-engine-env

lint:
	pre-commit run --all-files

configure:
	cmake --preset $(PRESET)

build:
	cmake --build --preset $(PRESET)

clean clear:
	rm -rf build/
