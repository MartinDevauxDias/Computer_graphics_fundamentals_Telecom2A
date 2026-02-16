# Project configuration
PROJECT_NAME = FinalProject
BUILD_DIR = build
CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Release

# Default target
.PHONY: all
all: build

# Configure and build the project
.PHONY: build
build: $(BUILD_DIR)/Makefile
	@echo "Building $(PROJECT_NAME)..."
	$(MAKE) -C $(BUILD_DIR) -j$(shell nproc)
	@echo "Build complete!"

# Configure CMake (only runs if CMakeLists.txt is newer than build files)
$(BUILD_DIR)/Makefile: CMakeLists.txt
	@echo "Configuring CMake..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. $(CMAKE_FLAGS)

# Run the executable
.PHONY: run
run: build
	@echo "Running $(PROJECT_NAME)..."
	./$(BUILD_DIR)/$(PROJECT_NAME)

# Debug build
.PHONY: debug
debug:
	@echo "Building debug version..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug
	$(MAKE) -C $(BUILD_DIR) -j$(shell nproc)

# Clean build files
.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

# Clean and rebuild
.PHONY: rebuild
rebuild: clean build

# Install dependencies (Ubuntu/Debian)
.PHONY: install-deps
install-deps:
	@echo "Installing dependencies..."
	sudo apt update
	sudo apt install -y cmake build-essential pkg-config
	sudo apt install -y libgl1-mesa-dev libglu1-mesa-dev
	sudo apt install -y libglfw3-dev libglew-dev
	sudo apt install -y libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all         - Build the project (default)"
	@echo "  build       - Configure and build"
	@echo "  run         - Build and run the executable"
	@echo "  debug       - Build debug version"
	@echo "  clean       - Remove build directory"
	@echo "  rebuild     - Clean and rebuild"
	@echo "  install-deps- Install system dependencies"
	@echo "  help        - Show this help message"

# Check if CMakeLists.txt exists
ifeq ($(wildcard CMakeLists.txt),)
$(error CMakeLists.txt not found. Make sure you're in the project root directory)
endif