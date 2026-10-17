CC = gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c11 -Isrc
SQLITE_FLAGS ?= -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_RTREE -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 -w

# OS detection
ifeq ($(OS),Windows_NT)
    TARGET = cvault.exe
    LDFLAGS ?= -lpthread
    RM = del /Q /F
    RMR = rmdir /S /Q
    FIXPATH = $(subst /,\,$1)
else
    TARGET = cvault
    LDFLAGS ?= -lpthread -ldl -lm
    RM = rm -f
    RMR = rm -rf
    FIXPATH = $1
endif

# Source files (excluding sqlite3.c for special compilation flags)
APP_SRCS = $(filter-out src/sqlite3.c, $(wildcard src/*.c))
APP_OBJS = $(APP_SRCS:.c=.o)
SQLITE_OBJ = src/sqlite3.o
ALL_OBJS = $(APP_OBJS) $(SQLITE_OBJ)

.PHONY: all clean run serve

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $(ALL_OBJS) $(LDFLAGS) -o $@
	@echo [OK] Built $(TARGET) successfully.

# Application objects compiled with full warnings enabled
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# SQLite compiled separately with optimizations and third-party warnings suppressed
$(SQLITE_OBJ): src/sqlite3.c
	$(CC) $(SQLITE_FLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

serve: $(TARGET)
	python dev_server.py

clean:
ifeq ($(OS),Windows_NT)
	-@$(RM) $(call FIXPATH,$(ALL_OBJS)) 2>nul || exit 0
	-@$(RM) $(TARGET) 2>nul || exit 0
else
	@$(RM) $(ALL_OBJS) $(TARGET)
endif
	@echo [OK] Cleaned build artifacts.
