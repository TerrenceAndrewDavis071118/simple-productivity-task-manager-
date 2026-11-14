CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -s
PKGS := gtk4 libnotify

TARGET := build/task-killer
SRC := src/main.cpp

.PHONY: all clean deb test

all: $(TARGET)

$(TARGET): $(SRC) src/store.hpp src/style.hpp src/audio.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $$(pkg-config --cflags --libs $(PKGS))

test: build/test_store_and_alerts
	./build/test_store_and_alerts

build/test_store_and_alerts: tests/test_store_and_alerts.cpp src/store.hpp src/audio.hpp
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall -Wextra tests/test_store_and_alerts.cpp -o build/test_store_and_alerts $$(pkg-config --cflags --libs glib-2.0)

deb: all
	./packaging/build-deb.sh

clean:
	rm -rf build dist
