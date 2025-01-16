#CC := gcc
CC := clang
#CC := zig cc
CC_OPTS := -std=c23 -g -O3 -Werror -Wall -Wextra -Wconversion -pedantic # -fsanitize=undefined,address
INCLUDE_DIRS := external/inc
LINKER_OPTS := -lc -lm #-lasan -lubsan


util_json_test: util_json_test.c util.c
	$(CC) $(CC_OPTS) -o util_json_test util_json_test.c $(LINKER_OPTS)

clean:
	rm -f util_json_test
