CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -s
PKGS := gtk4 libnotify

TARGET := build/task-killer
SRC := src/main.cpp

.PHONY: all clean deb

all: $(TARGET)

$(TARGET): $(SRC) src/store.hpp src/style.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $$(pkg-config --cflags --libs $(PKGS))

deb: all
	./packaging/build-deb.sh

clean:
	rm -rf build dist
