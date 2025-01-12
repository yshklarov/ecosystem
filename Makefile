PROJECT := ecosystem
CC := "clang++"
CC_OPTS := -std=c++20 -g -O3 -Wall -Wextra -pedantic -Wno-missing-field-initializers
INCLUDE_DIRS := external/inc
LINKER_OPTS := -lX11 -lc -lm

$(PROJECT): $(PROJECT).cpp
	$(CC) $(CC_OPTS) -isystem $(INCLUDE_DIRS) -o $(PROJECT) $(PROJECT).cpp $(LINKER_OPTS)

clean:
	rm $(PROJECT)
