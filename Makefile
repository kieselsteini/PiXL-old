CC=cc -O2 -Wall -Wextra `sdl2-config --cflags` -DLUA_USE_POSIX
LIB=`sdl2-config --libs` -lm -ldl
OBJ=lua53.o pixl.o
BIN=pixl

default: $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(LIB)

clean:
	rm -f $(BIN) $(OBJ)

