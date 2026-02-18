BUILD_DIR := build

all: configure build

configure:
	cmake -S . -B $(BUILD_DIR)

build:
	cmake --build $(BUILD_DIR)

run:
	cmake --build $(BUILD_DIR) --target run

clean:
	cmake --build $(BUILD_DIR) --target clean

format:
	cmake -E echo "Formatting sources..."
	clang-format -i src/*.c include/*.h

.PHONY: all configure build run clean format