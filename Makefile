CC      = cc
CFLAGS  = -std=c11 -O2 -Wall -Wextra
LDFLAGS = -lm
SDL     = $(shell pkg-config --cflags --libs sdl3)
LUA     = $(shell pkg-config --cflags --libs lua5.4)

BIN     = sdl_game

SRC     = main.c lua_cam.c
VERT    = cube.vert.glsl
FRAG    = cube.frag.glsl
VSPV    = cube.vert.spv
FSPV    = cube.frag.spv

all: $(BIN)

$(BIN): $(SRC) $(VSPV) $(FSPV)
	$(CC) $(CFLAGS) $(SRC) $(SDL) $(LUA) $(LDFLAGS) -o $@

$(VSPV): $(VERT)
	glslangValidator -V $< -o $@

$(FSPV): $(FRAG)
	glslangValidator -V $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) $(VSPV) $(FSPV)

.PHONY: all run clean

