CC = gcc 
CFLAGS = -g -march=native -fomit-frame-pointer -Wall -Wextra
SRC = $(shell find . -name '*.c')
OBJ = $(SRC:.c=.o)
TARGET = driver
ifdef DEBUG
    CFLAGS += -DDEBUG -O0
    $(info Building with DEBUG enabled, may cause performance issues)
else
    CFLAGS += -O3
endif
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	find . -name '*.o' -delete
	rm -f $(TARGET)

.PHONY: all clean