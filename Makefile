CC ?= cc
PYTHON ?= python3
.DEFAULT_GOAL := all

KILIX_GAME_KIT_DIR ?= third_party/kilix-game-kit
KILIX_GAME_KIT_ROOT := $(abspath $(KILIX_GAME_KIT_DIR))
include $(KILIX_GAME_KIT_DIR)/mk/game-kit.mk

KILIX_TOP_DOWN_DIR ?= third_party/kilix-top-down-engine
include $(KILIX_TOP_DOWN_DIR)/mk/kilix-top-down.mk

KILIX_ASSETS_DIR ?= third_party/kilix-assets
include $(KILIX_ASSETS_DIR)/mk/kilix-assets.mk

KILIX_UI_DIR ?= third_party/kilix-ui
include $(KILIX_UI_DIR)/mk/kilix-ui.mk

# The conservatory embeds pleb-plant-grower's archive. Host and archive share
# aggregate layouts (its docs/EMBEDDING.md), so both must be built from one
# kilix-game-sdk checkout: exporting the directory makes the archive's
# sub-make compile against Land's SDK instead of its own nested copy, and
# sdk-check confirms that is the revision pleb-plant-grower itself pins.
export KILIX_GAME_SDK_DIR := $(abspath third_party/kilix-game-sdk)
KILIX_PLANT_DIR ?= third_party/pleb-plant-grower
PG_BUILD_DIR := $(abspath build/pleb-plant-grower)
include $(KILIX_PLANT_DIR)/mk/pleb-plant-grower.mk
# The plant's sound bank and its three procedural atlases are build products
# it keeps out of git: its own generators rebuild them byte for byte, and its
# .gitignore keeps them out of the submodule's status.
PG_ASSET_STAMP := $(PG_BUILD_DIR)/assets.stamp

override CPPFLAGS += -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
	$(KILIX_GAME_KIT_CPPFLAGS) $(KILIX_TD_CPPFLAGS) \
	$(KILIX_ASSETS_CPPFLAGS) $(KILIX_UI_CPPFLAGS) $(PG_CPPFLAGS)
WARNINGS := -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
	-Wstrict-prototypes -Wmissing-prototypes -Wformat=2
SANITIZE_CFLAGS := -O1 -g -std=c11 -pthread $(WARNINGS) \
	-fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS ?= -O2 -g
override CFLAGS += -std=c11 -pthread $(WARNINGS) -MMD -MP
LDLIBS := $(KILIX_ASSETS_LDLIBS) $(KILIX_GAME_KIT_LDLIBS)

BIN := kilix-land
SRC := src/main.c src/game.c src/graphics.c src/render.c src/audio.c \
	src/conservatory.c
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
DEPENDENCIES := $(OBJ:.o=.d)
GRAPHICS_MANIFEST := assets/graphics/manifest.json
GAMES_ROOT ?= ..
PARITY_TOOL := tools/sync_source_parity.py

all: $(BIN) $(PG_ASSET_STAMP)

$(BIN): $(OBJ) $(GRAPHICS_MANIFEST) $(PG_LIB) $(KILIX_UI_LIB) \
	$(KILIX_TD_LIBS) $(KILIX_ASSETS_LIB) $(KILIX_GAME_KIT_LIB)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(PG_LIB) $(KILIX_UI_LIB) $(KILIX_TD_LIBS) \
		$(KILIX_ASSETS_LIB) $(KILIX_GAME_KIT_LIB) $(LDLIBS)

build/%.o: src/%.c src/kilix_land.h src/source_parity.h \
	$(KILIX_ASSETS_ROOT)/include/kilix_assets.h \
	$(KILIX_UI_ROOT)/include/kilix_ui.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p $@

$(PG_ASSET_STAMP): $(KILIX_PLANT_DIR)/tools/gen_sfx.py \
	$(KILIX_PLANT_DIR)/tools/gen_art.py \
	$(KILIX_PLANT_DIR)/docs/audio-provenance.json
	$(MAKE) -C $(KILIX_PLANT_DIR) PYTHON="$(PYTHON)" sfx art
	mkdir -p $(dir $@)
	touch $@

parity-sync:
	$(PYTHON) $(PARITY_TOOL) --games-root "$(abspath $(GAMES_ROOT))" --write

parity-check:
	$(PYTHON) $(PARITY_TOOL) --games-root "$(abspath $(GAMES_ROOT))"

# Two checkouts that disagree would mean the archive was tested by its own
# project against a different SDK than the one it is linked with here.
sdk-check:
	@land=$$(git -C third_party/kilix-game-sdk rev-parse HEAD 2>/dev/null); \
	plant=$$(git -C $(KILIX_PLANT_DIR) rev-parse \
		HEAD:third_party/kilix-game-sdk 2>/dev/null); \
	if [ -z "$$land" ] || [ -z "$$plant" ]; then \
		echo "SKIP sdk-check: submodule history is not available"; \
	elif [ "$$land" != "$$plant" ]; then \
		echo "FAIL sdk-check: Land builds kilix-game-sdk $$land but" \
			"pleb-plant-grower pins $$plant" >&2; exit 1; \
	else \
		echo "PASS sdk-check: one kilix-game-sdk for both ($$land)"; \
	fi

test: parity-check sdk-check $(BIN) $(PG_ASSET_STAMP)
	./$(BIN) --selftest
	./$(BIN) --audio-test
	./$(BIN) --graphics-test
	./$(BIN) --menu-render-test build/menu-preview.ppm
	./$(BIN) --room-render-test build/room-preview.ppm
	./$(BIN) --render-test build/preview.ppm
	mkdir -p build/cast-preview
	./$(BIN) --cast-render-test build/cast-preview
	mkdir -p build/training-preview
	./$(BIN) --training-render-test build/training-preview
	mkdir -p build/walk-preview
	./$(BIN) --walk-render-test build/walk-preview
	rm -rf build/conservatory-test
	mkdir -p build/conservatory-test
	./$(BIN) --conservatory-test build/conservatory-test
	mkdir -p build/conservatory-preview
	./$(BIN) --conservatory-render-test build/conservatory-preview

test-deps:
	$(MAKE) -C $(KILIX_GAME_KIT_ROOT) test
	$(MAKE) -C $(KILIX_TOP_DOWN_ROOT) \
		SOFT_RASTER_DIR="$(SOFT_RASTER_DIR)" test

sanitize:
	$(MAKE) clean
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 \
		$(MAKE) test CC=clang CFLAGS="$(SANITIZE_CFLAGS)"
	$(MAKE) -C $(KILIX_ASSETS_ROOT) test
	$(MAKE) -C $(KILIX_UI_ROOT) \
		KILIX_TOP_DOWN_DIR="$(KILIX_TOP_DOWN_ROOT)" \
		SOFT_RASTER_DIR="$(SOFT_RASTER_DIR)" test

clean:
	$(RM) -r build $(BIN)

.PHONY: all clean parity-check parity-sync sdk-check test test-deps sanitize

-include $(DEPENDENCIES)
