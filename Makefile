PROJECT := ecosystem
#CC := gcc
CC := clang
#CC := zig cc
CC_OPTS := -std=c23 -g -O3 -Wall -Wextra -Wconversion -pedantic -Wno-missing-field-initializers -fuse-ld=mold # -fsanitize=undefined,address
INCLUDE_DIRS := external/inc
LINKER_OPTS := -lX11 -lc -lm

$(PROJECT): $(PROJECT).c util/util.c
	$(CC) $(CC_OPTS) -isystem $(INCLUDE_DIRS) -o $(PROJECT) $(PROJECT).c $(LINKER_OPTS)

clean:
	rm -f $(PROJECT)
