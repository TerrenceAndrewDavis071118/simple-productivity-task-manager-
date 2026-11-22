CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -s

IS_WIN :=
ifeq ($(OS),Windows_NT)
    IS_WIN := 1
endif
ifneq ($(findstring MINGW,$(shell uname -s 2>/dev/null)),)
    IS_WIN := 1
endif
ifneq ($(findstring MSYS,$(shell uname -s 2>/dev/null)),)
    IS_WIN := 1
endif

ifeq ($(IS_WIN),1)
    TARGET := build/task-killer.exe
    TEST_TARGET := build/test_store_and_alerts.exe
    INSTALLER_TARGET := build/task-killer-installer.exe
    PKGS := gtk4
    LDFLAGS_EXTRA := -mwindows -lwinmm
    TEST_LDFLAGS_EXTRA := -lwinmm
else
    TARGET := build/task-killer
    TEST_TARGET := build/test_store_and_alerts
    INSTALLER_TARGET := build/task-killer-installer
    PKGS := gtk4 libnotify
    LDFLAGS_EXTRA :=
    TEST_LDFLAGS_EXTRA :=
endif

SRC := src/main.cpp
HEADERS := src/platform.hpp src/store.hpp src/style.hpp src/audio.hpp

.PHONY: all clean deb test zip run installer

all: $(TARGET)

installer: $(TARGET) $(INSTALLER_TARGET)

$(INSTALLER_TARGET): src/installer_main.cpp src/installer_ui.hpp $(TARGET)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) src/installer_main.cpp -o $(INSTALLER_TARGET) $$(pkg-config --cflags --libs gtk4) $(LDFLAGS_EXTRA)

run: all
	./$(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $$(pkg-config --cflags --libs $(PKGS)) $(LDFLAGS_EXTRA)

test: $(TEST_TARGET)
	$(TEST_TARGET)

$(TEST_TARGET): tests/test_store_and_alerts.cpp src/platform.hpp src/store.hpp src/audio.hpp
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall -Wextra tests/test_store_and_alerts.cpp -o $(TEST_TARGET) $$(pkg-config --cflags --libs glib-2.0) $(TEST_LDFLAGS_EXTRA)

deb: all
	./packaging/build-deb.sh

zip: all
	./packaging/build-windows-zip.sh

clean:
	rm -rf build dist
