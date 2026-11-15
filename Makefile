NAME        = elisapp

# Cross-compilation ARM : make CROSS=aarch64-linux-gnu- SDL_CONFIG=/chemin/sysroot/usr/bin/sdl2-config
# (ou arm-linux-gnueabihf- pour ARM 32 bits). Vide = build natif.
CROSS      ?=
SDL_CONFIG ?= sdl2-config

# Folders
APP_DIR     = app
LV_DIR      = lvgl
TMP_DIR     = .tmp$(if $(CROSS),-$(CROSS:-=))

# Sources
SRC         = $(sort $(shell find $(APP_DIR) -name '*.cpp' 2>/dev/null))
LV_SRC      = $(sort $(shell find $(LV_DIR)/src -name '*.c' 2>/dev/null))
OBJS        = $(patsubst %.cpp, $(TMP_DIR)/%.o, $(SRC)) \
              $(patsubst %.c,   $(TMP_DIR)/%.o, $(LV_SRC))
DEPS        = $(OBJS:.o=.d)

# Compiler
CXX         = $(if $(CROSS),$(CROSS)g++,c++)
CC          = $(if $(CROSS),$(CROSS)gcc,cc)

# -O2: the map is drawn one sample per pixel (843 k a frame on the iPad mini)
CXXFLAGS    = -std=c++20 -O2 -g \
              -Wall -Wextra -Werror -Wpedantic \
              -Wshadow -Wconversion -Wsign-conversion

CFLAGS      = -O2 -g
DEPFLAGS    = -MMD -MP -fdiagnostics-color=always

SDL_PREFIX := $(shell $(SDL_CONFIG) --prefix 2>/dev/null)
INCLUDES    = -DLV_CONF_INCLUDE_SIMPLE \
              -I. -I$(APP_DIR) \
              -isystem $(LV_DIR) \
              -idirafter $(SDL_PREFIX)/include
LDFLAGS    := $(shell $(SDL_CONFIG) --libs 2>/dev/null)

JOBS       := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
MAKEFLAGS  += --no-print-directory
ifeq ($(MAKELEVEL),0)
    MAKEFLAGS += -j$(JOBS)
endif

# Colors
GREEN       = \033[1;38;2;0;255;0m
RED         = \033[1;38;2;255;0;0m
DEF_COLOR   = \033[0m

# Progress
CLEAR_LINE  = printf "\r\033[K"
SHOW_LOG    = if [ -s $@.log ]; then $(CLEAR_LINE); cat $@.log; fi; rm -f $@.log
PROGRESS    = printf "\r%3d%%" $$(( $$(find $(TMP_DIR) -name '*.o' | wc -l) * 100 / $(words $(OBJS)) ))

# Checks
HASH       := \#
ifeq ($(filter clean fclean,$(MAKECMDGOALS)),)
    ifeq ($(SRC),)
        $(error No .cpp found in $(APP_DIR)/)
    endif
    ifeq ($(LV_SRC),)
        $(error LVGL not found in $(LV_DIR)/src)
    endif
    ifeq ($(wildcard lv_conf.h),)
        $(error lv_conf.h not found)
    endif
    ifneq ($(filter 0,$(word 2,$(shell grep -m1 '^$(HASH)if' lv_conf.h))),)
        $(error lv_conf.h disabled: set its first '#if 0' to '#if 1')
    endif
    ifeq ($(SDL_PREFIX),)
        $(error $(SDL_CONFIG) not found)
    endif
endif

# Rules

all: build
ifeq ($(CROSS),)
	@./$(NAME)
endif

build: $(NAME)

$(NAME): $(OBJS)
	@$(CLEAR_LINE)
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS) \
	    && printf "%b\n" "$(GREEN)Compilation successful !$(DEF_COLOR)" \
	    || (printf "%b\n" "$(RED)Error during linking!$(DEF_COLOR)" && exit 1)

$(TMP_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@ 2> $@.log \
	    || ($(SHOW_LOG); printf "%b\n" "$(RED)Error compiling $<!$(DEF_COLOR)"; exit 1)
	@$(SHOW_LOG)
	@$(PROGRESS)

$(TMP_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@ 2> $@.log \
	    || ($(SHOW_LOG); printf "%b\n" "$(RED)Error compiling $<!$(DEF_COLOR)"; exit 1)
	@$(SHOW_LOG)
	@$(PROGRESS)

$(OBJS): Makefile

ifeq ($(filter clean fclean,$(MAKECMDGOALS)),)
    -include $(DEPS)
endif

clean:
	@rm -rf $(TMP_DIR)
	@printf "%b\n" "$(GREEN)Clean successful !$(DEF_COLOR)"

fclean: clean
	@rm -f $(NAME)
	@printf "%b\n" "$(GREEN)Full clean successful !$(DEF_COLOR)"

re:
	@$(MAKE) fclean
	@$(MAKE) build

# Data module (app/data) must stay light: no heap, stdio, libm, exceptions,
# no float->double promotion, small stack frames. sim/ is PC only, unchecked.
# Objects are .obj so the build progress (which counts *.o) ignores them.
DATA_DIR    = $(APP_DIR)/data
DATA_SRC    = $(sort $(shell find $(DATA_DIR) -name '*.cpp' 2>/dev/null))
EMBED_SRC   = $(filter-out $(DATA_DIR)/sim/%,$(DATA_SRC))
EMBED_DIR   = $(TMP_DIR)/embed
NM          = $(if $(CROSS),$(CROSS)nm,nm)
# Undefined symbols as Linux (ELF) names them: heap, stdio, libm, C++ runtime
EMBED_BAD   = ^ *U (malloc|calloc|realloc|free|_Zn[wa].*|_Zd[la].*|f?open|fread|fwrite|fclose|f?printf|puts|(a?sin|a?cos|a?tan|atan2|sqrt|exp|log|pow|hypot|floor|ceil|l?round)f?|__cxa_.*|__gxx_.*|_Unwind_.*)$$

check-embedded:
	@mkdir -p $(EMBED_DIR)
	@rm -f $(EMBED_DIR)/*.obj
	@for f in $(EMBED_SRC); do \
	    $(CXX) $(CXXFLAGS) -Os -fno-exceptions -fno-rtti -Wdouble-promotion \
	        -Wframe-larger-than=512 -I. -I$(APP_DIR) -c $$f \
	        -o $(EMBED_DIR)/$$(echo $$f | tr / _).obj \
	        || { printf "%b\n" "$(RED)Error compiling $$f!$(DEF_COLOR)"; exit 1; }; \
	done
	@if $(NM) -u $(EMBED_DIR)/*.obj | grep -E '$(EMBED_BAD)'; then \
	    printf "%b\n" "$(RED)Embedded check failed: heavy symbol above$(DEF_COLOR)"; \
	    exit 1; \
	fi
	@printf "%b\n" "$(GREEN)Embedded check OK (no heap, stdio, libm, exceptions)$(DEF_COLOR)"

# Data module tests (no LVGL/SDL), with sanitizers. Optional zone made by the
# python tool: make test ZONE=data/zone.bin [ZONE_FINE=data/ctx_layer.bin]
# ARM target through qemu:
#   make test CROSS=arm-linux-gnueabihf- TEST_SAN= \
#        TEST_RUN="qemu-arm -L /usr/arm-linux-gnueabihf"
TEST_SAN   ?= -fsanitize=address,undefined -fno-sanitize-recover=all
TEST_RUN   ?=

test: check-embedded
	@mkdir -p $(TMP_DIR)
	@$(CXX) $(CXXFLAGS) $(TEST_SAN) -I. -I$(APP_DIR) tests/data_test.cpp \
	    $(DATA_SRC) -o $(TMP_DIR)/data_test \
	    || (printf "%b\n" "$(RED)Error compiling tests!$(DEF_COLOR)" && exit 1)
	@$(TEST_RUN) ./$(TMP_DIR)/data_test $(ZONE) $(ZONE_FINE)

# Application tests without a screen: guidance, simulated GPS, the planner on
# the data module (sanitizers on, like `make test`)
APP_TEST_SRC = $(APP_DIR)/guidance/guidance.cpp $(APP_DIR)/guidance/course.cpp $(APP_DIR)/platform/gps_sim.cpp \
               $(sort $(shell find $(APP_DIR)/integration $(APP_DIR)/core/pathfinding $(APP_DIR)/core/json $(APP_DIR)/core/route -name '*.cpp' 2>/dev/null)) \
               $(DATA_SRC)

test-app:
	@mkdir -p $(TMP_DIR)
	@$(CXX) $(CXXFLAGS) $(TEST_SAN) -I. -I$(APP_DIR) -Itests tests/guidance_test.cpp \
	    $(APP_TEST_SRC) -o $(TMP_DIR)/guidance_test \
	    || (printf "%b\n" "$(RED)Error compiling tests!$(DEF_COLOR)" && exit 1)
	@$(TEST_RUN) ./$(TMP_DIR)/guidance_test

# The whole screen on a display without a window, from the objects of the
# build: every app source but main.cpp and the SDL backend
UI_TEST_SRC  = $(filter-out $(APP_DIR)/main.cpp $(APP_DIR)/platform/SdlDisplayBackend.cpp,$(SRC))
UI_TEST_OBJS = $(patsubst %.cpp, $(TMP_DIR)/%.o, $(UI_TEST_SRC)) \
               $(patsubst %.c,   $(TMP_DIR)/%.o, $(LV_SRC))

test-ui: $(UI_TEST_OBJS)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -Itests tests/ui_smoke_test.cpp $(UI_TEST_OBJS) \
	    -o $(TMP_DIR)/ui_smoke_test $(LDFLAGS) \
	    || (printf "%b\n" "$(RED)Error compiling tests!$(DEF_COLOR)" && exit 1)
	@$(TEST_RUN) ./$(TMP_DIR)/ui_smoke_test

# Real data of the site in assets/mission_site.json (data/ is git ignored):
# relief down to 2 m, orbital images, Mars Climate Database weather. The tool
# runs in its own virtual environment (numpy, tifffile, imagecodecs).
TOOLS_VENV  = .venv-tools
data: $(TOOLS_VENV)/.ready
	@$(TOOLS_VENV)/bin/python tools/fetch_site.py

$(TOOLS_VENV)/.ready: tools/requirements.txt
	@python3 -m venv $(TOOLS_VENV)
	@$(TOOLS_VENV)/bin/pip install -q -r tools/requirements.txt
	@touch $@

.DELETE_ON_ERROR:

.PHONY: all build clean fclean re test check-embedded test-app test-ui data
