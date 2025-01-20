PROJECT := ecosystem
CC := clang
#CC := gcc
#CC := zig cc
OPTIMIZE_OPTS := -O3 -flto
SANITIZE_OPTS := #-fsanitize=undefined,address
CC_OPTS := -std=c17 -g3 $(OPTIMIZE_OPTS) -Wall -Wextra -Wconversion -pedantic -Wno-missing-field-initializers -fuse-ld=mold $(SANITIZE_OPTS)
INCLUDE_DIRS := external/inc
LINKER_OPTS := -lX11 -lc -lm #-lasan -lubsan

$(PROJECT): $(PROJECT).c util/util.c
	$(CC) $(CC_OPTS) -isystem $(INCLUDE_DIRS) -o $(PROJECT) $(PROJECT).c $(LINKER_OPTS)

util_json_test: util/util_json_test.c util/util.c
	$(CC) $(CC_OPTS) -o util/util_json_test util/util_json_test.c -lc -lm

test: util_json_test

clean:
	rm -f $(PROJECT) util/util_json_test
