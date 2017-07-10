CFLAGS = -g3 -O0 -Wall
SHARED := -fPIC --shared
INC = include
SRC = src
BUILD = build

all: $(BUILD)/areasearch.so

$(BUILD):
	mkdir $(BUILD)

$(BUILD)/areasearch.so: $(SRC)/lua-areasearch.c $(SRC)/divgrid.c | $(BUILD)
	gcc $(CFLAGS) $(SHARED) $^ -o $@ -I$(INC)

run:
	bin/lua test.lua

clean:
	rm $(BUILD)/*