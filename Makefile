OPTFLAGS   = -Wall
SDL_LIBS   = `sdl2-config --static-libs`
SDL_CFLAGS = `sdl2-config --cflags`
CFLAGS     = $(OPTFLAGS)  $(SDL_CFLAGS)
LDFLAGS    = $(SDL_LIBS) 

CSRC       = yay.c
TARGET     = yay
OBJ        = $(CSRC:.c=.o)

default: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS)  -g -c -o $@ $<

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm $(OBJ) $(TARGET)

