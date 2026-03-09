# Makefile for liboas Podman Container Management
#
# Quick Start:
#   make build-dev     # Build development image (~15 min first time)
#   make dev           # Start interactive development shell
#   make test          # Build test image and run tests
#   make quality       # Run full quality pipeline
#   make clean         # Remove all liboas images
#
# For detailed usage: make help

.PHONY: help build-dev build-test dev test quality shell info \
        clean clean-images rebuild-dev version

# Configuration
PROJECT_NAME := liboas
PROJECT_ROOT := $(shell cd ../.. && pwd)
DEV_IMAGE := localhost/$(PROJECT_NAME)-dev:latest
TEST_IMAGE := localhost/$(PROJECT_NAME)-test:latest

# Colors
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m

.DEFAULT_GOAL := help

##@ General

help: ## Display this help message
	@echo "$(GREEN)liboas Container Management$(NC)"
	@echo ""
	@awk 'BEGIN {FS = ":.*##"; printf "Usage:\n  make $(YELLOW)<target>$(NC)\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  $(YELLOW)%-20s$(NC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(BLUE)%s$(NC)\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Build Images

build-dev: ## Build development image (standalone from oraclelinux:10)
	@echo "$(YELLOW)Building development image...$(NC)"
	podman build -t $(DEV_IMAGE) -f Containerfile.dev .
	@echo "$(GREEN)Development image built: $(DEV_IMAGE)$(NC)"

build-test: ## Build test image (pre-built project, requires dev image)
	@echo "$(YELLOW)Building test image...$(NC)"
	podman build -t $(TEST_IMAGE) -f Containerfile.test $(PROJECT_ROOT)
	@echo "$(GREEN)Test image built: $(TEST_IMAGE)$(NC)"

##@ Run Containers

dev: ## Start interactive development shell
	@echo "$(YELLOW)Starting development shell...$(NC)"
	podman run --rm -it \
		--security-opt seccomp=unconfined \
		-v $(PROJECT_ROOT):/workspace:Z \
		$(DEV_IMAGE)

test: build-test ## Build and run tests
	@echo "$(YELLOW)Running tests...$(NC)"
	podman run --rm $(TEST_IMAGE)

quality: ## Run full quality pipeline (mount source, not pre-built)
	@echo "$(YELLOW)Running quality pipeline...$(NC)"
	podman run --rm \
		--security-opt seccomp=unconfined \
		--env-file $(PROJECT_ROOT)/.env \
		-v $(PROJECT_ROOT):/workspace:Z \
		$(DEV_IMAGE) \
		bash -c "cd /workspace && ./scripts/quality.sh"

shell: dev ## Alias for dev

##@ Development Workflow

build-run: ## Build and run tests in one step (mount source)
	@echo "$(YELLOW)Build + test...$(NC)"
	podman run --rm \
		--security-opt seccomp=unconfined \
		-v $(PROJECT_ROOT):/workspace:Z \
		$(DEV_IMAGE) \
		bash -c "cd /workspace && \
			cmake --preset clang-debug -DOAS_YAML=ON && \
			cmake --build --preset clang-debug && \
			ctest --preset clang-debug --output-on-failure"

format: ## Run clang-format check
	@echo "$(YELLOW)Checking formatting...$(NC)"
	podman run --rm \
		-v $(PROJECT_ROOT):/workspace:Z \
		$(DEV_IMAGE) \
		bash -c "cd /workspace && \
			cmake --preset clang-debug && \
			cmake --build --preset clang-debug --target format-check"

##@ Cleanup

clean: clean-images ## Remove all liboas images

clean-images: ## Remove all built images
	@echo "$(YELLOW)Removing liboas images...$(NC)"
	podman rmi -f $(DEV_IMAGE) $(TEST_IMAGE) 2>/dev/null || true
	@echo "$(GREEN)Images cleaned$(NC)"

rebuild-dev: clean-images build-dev ## Rebuild dev image from scratch

##@ System Information

info: ## Display container and tool information
	@echo "$(BLUE)System:$(NC)"
	@echo "  Podman: $$(podman --version 2>/dev/null || echo 'not installed')"
	@echo ""
	@echo "$(BLUE)Images:$(NC)"
	@podman images | grep $(PROJECT_NAME) || echo "  No images found. Run: make build-dev"
	@echo ""
	@echo "$(BLUE)Dev image tools (if built):$(NC)"
	@podman run --rm $(DEV_IMAGE) bash -c "\
		echo \"  Clang:     \$$(clang --version | head -1)\" && \
		echo \"  GCC-15:    \$$(gcc-15 --version | head -1)\" && \
		echo \"  CMake:     \$$(cmake --version | head -1)\" && \
		echo \"  cppcheck:  \$$(cppcheck --version)\" && \
		echo \"  Doxygen:   \$$(doxygen --version)\" && \
		echo \"  PVS:       \$$(pvs-studio --version 2>&1 | head -1)\" && \
		echo \"  mold:      \$$(mold --version)\"" 2>/dev/null || \
	echo "  (dev image not built yet)"

version: ## Show project version info
	@echo "$(PROJECT_NAME) development container"
	@echo "Base: oraclelinux:10"
	@echo "Compilers: Clang 22.1.0, GCC 15.1.1"
	@echo "Libraries: yyjson 0.12, PCRE2 10.45, libfyaml 0.9"
