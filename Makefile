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

CXXFLAGS    = -std=c++20 -O0 -g \
              -Wall -Wextra -Werror -Wpedantic \
              -Wshadow -Wconversion -Wsign-conversion

CFLAGS      = -O0 -g
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

# Data module must stay MCU friendly (STM32): no heap, stdio, libm, exceptions,
# no float->double, small stack frames. weather_sim is PC only, not checked.
EMBED_SRC   = $(filter-out %_sim.cpp,$(sort $(wildcard $(APP_DIR)/data/*.cpp)))
EMBED_BAD   = malloc|_Znw|_Zna|_Zdl|fopen|fread|fclose|printf|_cos|_sin|_atan|_sqrt|_hypot|__cxa_|__gxx_

check-embedded:
	@mkdir -p $(TMP_DIR)/embed
	@for f in $(EMBED_SRC); do \
	    $(CXX) $(CXXFLAGS) -Os -fno-exceptions -fno-rtti -Wdouble-promotion \
	        -Wframe-larger-than=512 -I. -I$(APP_DIR) -c $$f \
	        -o $(TMP_DIR)/embed/$$(basename $$f .cpp).o \
	        || { printf "%b\n" "$(RED)Error compiling $$f!$(DEF_COLOR)"; exit 1; }; \
	done
	@if nm -u $(TMP_DIR)/embed/*.o | grep -E -i '$(EMBED_BAD)'; then \
	    printf "%b\n" "$(RED)Embedded check failed: heavy symbol above$(DEF_COLOR)"; \
	    exit 1; \
	fi
	@printf "%b\n" "$(GREEN)Embedded check OK (no heap, stdio, libm, exceptions)$(DEF_COLOR)"

# Data module tests (no LVGL/SDL). Optional: make test ZONE=data/zone.bin
test: check-embedded
	@mkdir -p $(TMP_DIR)
	@$(CXX) $(CXXFLAGS) -I. -I$(APP_DIR) tests/data_test.cpp \
	    $(sort $(wildcard $(APP_DIR)/data/*.cpp)) -o $(TMP_DIR)/data_test \
	    || (printf "%b\n" "$(RED)Error compiling tests!$(DEF_COLOR)" && exit 1)
	@./$(TMP_DIR)/data_test $(ZONE)

.DELETE_ON_ERROR:

.PHONY: all build clean fclean re test check-embedded
