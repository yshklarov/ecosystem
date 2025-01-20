PROJECT := ecosystem
BUILD_ROOT := ./build
CC := clang
#CC := gcc
#CC := zig cc
OPTIMIZE_OPTS := -O3 -flto
SANITIZE_OPTS := #-fsanitize=undefined,address
CC_OPTS := -std=c17 -g3 $(OPTIMIZE_OPTS) -Wall -Wextra -Wconversion -pedantic -Wno-missing-field-initializers -fuse-ld=mold $(SANITIZE_OPTS)
INCLUDE_DIRS := external/inc
LINKER_OPTS := -lX11 -lc -lm #-lasan -lubsan

$(PROJECT): src/$(PROJECT).c src/util.c
	mkdir -p $(BUILD_ROOT) && \
	$(CC) $(CC_OPTS) -isystem $(INCLUDE_DIRS) -o $(BUILD_ROOT)/$(PROJECT) src/$(PROJECT).c $(LINKER_OPTS)

util_json_test: src/util_json_test.c src/util.c
	mkdir -p $(BUILD_ROOT) && \
	$(CC) $(CC_OPTS) -o $(BUILD_ROOT)/util_json_test src/util_json_test.c $(LINKER_OPTS)

clean:
	rm -f build/*
