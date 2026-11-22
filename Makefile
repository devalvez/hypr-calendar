# Atalhos sem CMake.
#
#   make preview   compila o calendar-preview (só precisa de g++ e pangocairo)
#   make test      compila e gera build/exemplo.png a partir de examples/preview.conf
#   make plugin    compila build/hypr-calendar.so (precisa dos headers do Hyprland)
#   make install   copia o .so para ~/.local/share/hypr-calendar/ e mostra como carregar
#   make clean
#
# Compatível com o Hyprland v0.57.x (testado no commit 6d6e935c3, v0.57.0-164).
# O compilador precisa ser o mesmo "nível" usado para compilar o Hyprland
# (GCC 14+ ou Clang recente). Para forçar outro: make plugin CXX=g++-14
# Para trocar o padrão C++: make plugin CXXSTD=c++2c

CXX      ?= g++
CXXSTD   ?= c++23
BUILD    := build
CORE_SRC := src/calendar_core.cpp

PLUGIN_DIR ?= $(HOME)/.local/share/hypr-calendar

PANGO_FLAGS := $(shell pkg-config --cflags --libs pangocairo)

.PHONY: all preview test plugin install clean

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
	$(CXX) -shared -fPIC --no-gnu-unique -std=$(CXXSTD) -O2 -g \
		$(CORE_SRC) src/main.cpp -o $@ \
		$(shell pkg-config --cflags hyprland pixman-1 libdrm) $(PANGO_FLAGS)

install: plugin
	@mkdir -p $(PLUGIN_DIR)
	cp $(BUILD)/hypr-calendar.so $(PLUGIN_DIR)/hypr-calendar.so
	@echo
	@echo "Instalado em $(PLUGIN_DIR)/hypr-calendar.so"
	@echo "Para carregar agora:   hyprctl plugin load $(PLUGIN_DIR)/hypr-calendar.so"
	@echo "Para carregar sempre, no hyprland.lua:"
	@echo "  hl.plugin.load(\"$(PLUGIN_DIR)/hypr-calendar.so\")"

clean:
	rm -rf $(BUILD)
