CC      = cc
CFLAGS  = -std=c11 -O2 -Wall -Wextra
LDFLAGS = -lm
SDL     = $(shell pkg-config --cflags --libs sdl3)

BIN     = cube

SRC     = cube.c
VERT    = cube.vert.glsl
FRAG    = cube.frag.glsl
VSPV    = cube.vert.spv
FSPV    = cube.frag.spv

all: $(BIN)

$(BIN): $(SRC) $(VSPV) $(FSPV)
	$(CC) $(CFLAGS) $(SRC) $(SDL) $(LDFLAGS) -o $@

$(VSPV): $(VERT)
	glslangValidator -V $< -o $@

$(FSPV): $(FRAG)
	glslangValidator -V $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) $(VSPV) $(FSPV)

.PHONY: all run clean

