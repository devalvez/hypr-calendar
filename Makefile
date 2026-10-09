# Atalhos sem CMake.
#
#   make preview   compila o calendar-preview (só precisa de g++ e pangocairo)
#   make test      compila e gera build/exemplo.png a partir de examples/preview.conf
#   make plugin    compila hypr-calendar.so (precisa dos headers do Hyprland)
#   make clean

CXX      ?= g++
BUILD    := build
CORE_SRC := src/calendar_core.cpp

PANGO_FLAGS := $(shell pkg-config --cflags --libs pangocairo)

.PHONY: all preview test plugin clean

all: preview

preview: $(BUILD)/calendar-preview

$(BUILD)/calendar-preview: $(CORE_SRC) src/preview.cpp src/calendar_core.hpp
	@mkdir -p $(BUILD)
	$(CXX) -std=c++20 -O2 -Wall -Wextra $(CORE_SRC) src/preview.cpp -o $@ $(PANGO_FLAGS)

test: preview
	$(BUILD)/calendar-preview -c examples/preview.conf --date 2026-08-15 --scale 2 -o $(BUILD)/exemplo.png

plugin: $(BUILD)/hypr-calendar.so

$(BUILD)/hypr-calendar.so: $(CORE_SRC) src/main.cpp src/calendar_core.hpp
	@mkdir -p $(BUILD)
	$(CXX) -shared -fPIC --no-gnu-unique -std=c++2b -O2 -g \
		$(CORE_SRC) src/main.cpp -o $@ \
		$(shell pkg-config --cflags hyprland pixman-1 libdrm) $(PANGO_FLAGS)

clean:
	rm -rf $(BUILD)
