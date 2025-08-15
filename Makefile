# Compiler and flags
LOG_LEVEL 	:= LOG_LEVEL_DEBUG  # Possible values: DEBUG, INFO, WARNING, ERROR
CC      	:= gcc
CFLAGS  	:= -ggdb -Wall -Wextra -std=c99 -I src -I main -DLOG_LEVEL=$(LOG_LEVEL)
AR      	:= ar rcs

# Detect OS and set libraries
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin) # macOS
    LIBS := -lglfw -lGLEW -framework OpenGL -lm
else # Linux and others
    LIBS := -lglfw -lGLEW -lGL -lm
endif

# Directories
SRC_DIR   := src
MAIN_DIR  := main
BUILD_DIR := build

# Main and top-level sources
MAIN_SRC  := $(wildcard $(MAIN_DIR)/*.c)
MAIN_OBJ  := $(patsubst $(MAIN_DIR)/%.c,$(BUILD_DIR)/%.o,$(MAIN_SRC))

TOP_SRC   := $(wildcard $(SRC_DIR)/*.c)
TOP_OBJ   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(TOP_SRC))

# Inner library directories
LIB_DIRS  := $(filter-out $(SRC_DIR), $(wildcard $(SRC_DIR)/*/))
LIB_NAMES := $(notdir $(patsubst %/,%,$(LIB_DIRS)))
LIB_FILES := $(patsubst %,$(BUILD_DIR)/lib%.a,$(LIB_NAMES))

# Target
TARGET := tayira

# Default
all: $(TARGET)

# Link final executable
$(TARGET): $(MAIN_OBJ) $(TOP_OBJ) $(LIB_FILES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(MAIN_OBJ) $(TOP_OBJ) $(filter-out build/liblogger.a build/libdata_structures.a,$(LIB_FILES)) build/libdata_structures.a build/liblogger.a $(LIBS)

# Compile main and top-level
$(BUILD_DIR)/%.o: $(MAIN_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build each static library
define make-lib-rules
$(1)_SRC := $(wildcard $(SRC_DIR)/$(1)/*.c)
$(1)_OBJ := $$(patsubst $(SRC_DIR)/$(1)/%.c,$(BUILD_DIR)/$(1)/%.o,$$($(1)_SRC))

$$(BUILD_DIR)/lib$(1).a: $$($(1)_OBJ)
	$$(AR) $$@ $$^

$$(BUILD_DIR)/$(1)/%.o: $(SRC_DIR)/$(1)/%.c | $(BUILD_DIR)/$(1)
	$$(CC) $$(CFLAGS) -c $$< -o $$@

$(BUILD_DIR)/$(1):
	mkdir -p $$@
endef

$(foreach lib,$(LIB_NAMES),$(eval $(call make-lib-rules,$(lib))))

# Ensure build dir exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean
