NAME=ls

SRC=$(NAME).c
BIN=$(NAME)

# --------

CC=clang
CFLAGS=-Wall -Wextra -pedantic

RM=rm

# --------

all: $(BIN)

clean:
	$(RM) ./$(BIN) || true

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^


