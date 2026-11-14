NAME      := nasaSpaceApps
# Cross-compilation ARM : make CROSS=aarch64-linux-gnu- SDL_CONFIG=/chemin/sysroot/usr/bin/sdl2-config
# (ou arm-linux-gnueabihf- pour ARM 32 bits). Vide = build natif.
CROSS     ?=
SDL_CONFIG ?= sdl2-config
BUILD     := build$(if $(CROSS),-$(CROSS:-=))

CXX       := $(if $(CROSS),$(CROSS)g++,c++)
CC        := $(if $(CROSS),$(CROSS)gcc,cc)
CXXFLAGS  := -std=c++20 -O0 -g -Wall -Wextra -Werror -Wpedantic -Wshadow -Wconversion -Wsign-conversion
CFLAGS    := -O0 -g
# lvgl/ (sources) et lv_conf.h (racine) doivent exister ; -isystem => warnings des headers LVGL ignorés
INCLUDES  := -I. -Iapp -isystem lvgl -isystem $(shell $(SDL_CONFIG) --prefix)/include
LDFLAGS   := $(shell $(SDL_CONFIG) --libs)
DEPFLAGS   = -MMD -MP

# Toujours utiliser tous les coeurs (macOS: hw.ncpu, Linux: nproc)
JOBS      := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu)
MAKEFLAGS += -j$(JOBS)

CPP_SRCS  := $(wildcard app/*.cpp app/*/*.cpp app/*/*/*.cpp)
LV_SRCS   := $(shell find lvgl/src -name '*.c' 2>/dev/null)
OBJS      := $(CPP_SRCS:%.cpp=$(BUILD)/%.o) $(LV_SRCS:%.c=$(BUILD)/%.o)

all: build
# Un binaire ARM cross-compilé ne tourne pas sur la machine hôte
ifeq ($(CROSS),)
	./$(NAME)
endif

build: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# LVGL est une lib tierce en C : pas de -Werror dessus
$(BUILD)/lvgl/%.o: lvgl/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

fclean: clean
	rm -f $(NAME)

# Récursif : garantit que fclean est fini avant de recompiler en parallèle
re:
	$(MAKE) fclean
	$(MAKE) build

-include $(OBJS:.o=.d)

.PHONY: all build clean fclean re
