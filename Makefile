# Makefile for Nintendo DS Project
# Requires devkitARM

.SUFFIXES:

# Check for devkitARM configuration
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools

#---------------------------------------------------------------------------------
# Configuration
#---------------------------------------------------------------------------------
TARGET      := flashme
BUILD       := build
INCLUDES    := .

# Source directories
# ARM9: Includes NDS/ARM9 and the common NDS folder
SRC_DIR_9   := NDS/ARM9 NDS
# ARM7: Includes only NDS/ARM7
SRC_DIR_7   := NDS/ARM7

#---------------------------------------------------------------------------------
# Architecture & Flags
#---------------------------------------------------------------------------------
ARCH_9      := -mthumb -mthumb-interwork -march=armv5te -mtune=arm946e-s
ARCH_7      := -mthumb -mthumb-interwork -mcpu=arm7tdmi

# Common flags
BASE_CFLAGS := -Wall -O2 -fomit-frame-pointer -ffast-math \
               $(addprefix -I,$(INCLUDES))

# Specific flags
CFLAGS_9    := $(BASE_CFLAGS) $(ARCH_9) -DARM9
CFLAGS_7    := $(BASE_CFLAGS) $(ARCH_7) -DARM7

LDFLAGS_9   := -specs=ds_arm9.specs -g $(ARCH_9)
LDFLAGS_7   := -specs=ds_arm7.specs -g $(ARCH_7)

#---------------------------------------------------------------------------------
# Source Discovery
#---------------------------------------------------------------------------------
# Helper to find all .c, .cpp, and .s files in a list of directories
FIND_SOURCES = $(foreach dir,$(1),$(wildcard $(dir)/*.c $(dir)/*.cpp $(dir)/*.s))

SOURCES_9   := $(call FIND_SOURCES,$(SRC_DIR_9)) $(wildcard arm9.c memdump.c data.c libnds_stubs.c)
SOURCES_7   := $(call FIND_SOURCES,$(SRC_DIR_7)) $(wildcard arm7.c sound.c libnds_stubs.c miniprintf.c)

# Check if any source files were found
ifeq ($(strip $(SOURCES_9)),)
$(error No ARM9 source files (.c/.cpp/.s) found in directories: $(SRC_DIR_9). You cannot compile without implementation files)
endif
ifeq ($(strip $(SOURCES_7)),)
$(error No ARM7 source files (.c/.cpp/.s) found in directories: $(SRC_DIR_7). You cannot compile without implementation files)
endif

# Generate object file paths in the build directory
OBJS_9      := $(addprefix $(BUILD)/arm9/,$(addsuffix .o,$(basename $(SOURCES_9))))
OBJS_7      := $(addprefix $(BUILD)/arm7/,$(addsuffix .o,$(basename $(SOURCES_7))))

#---------------------------------------------------------------------------------
# Main Targets
#---------------------------------------------------------------------------------
.PHONY: all clean

all: $(TARGET).nds

# Combine ARM9 and ARM7 binaries into the final NDS ROM
$(TARGET).nds: $(TARGET).arm9 $(TARGET).arm7
	@echo "Building $@"
	@ndstool -c $@ -9 $(TARGET).arm9 -7 $(TARGET).arm7
	@echo "Done!"
	@sh MAKE_NDS.sh

clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD) $(TARGET).nds $(TARGET).arm9 $(TARGET).arm7 $(TARGET).arm9.elf $(TARGET).arm7.elf

#---------------------------------------------------------------------------------
# ARM9 Build Rules
#---------------------------------------------------------------------------------
$(TARGET).arm9: $(TARGET).arm9.elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).arm9.elf: $(OBJS_9)
	@echo "Linking ARM9..."
	@$(CC) $(OBJS_9) $(LDFLAGS_9) -o $@

$(BUILD)/arm9/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling ARM9 $<"
	@$(CC) $(CFLAGS_9) -c $< -o $@

$(BUILD)/arm9/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling ARM9 $<"
	@$(CXX) $(CFLAGS_9) -fno-rtti -fno-exceptions -c $< -o $@

$(BUILD)/arm9/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "Assembling ARM9 $<"
	@$(CC) $(CFLAGS_9) -c $< -o $@

#---------------------------------------------------------------------------------
# ARM7 Build Rules
#---------------------------------------------------------------------------------
$(TARGET).arm7: $(TARGET).arm7.elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).arm7.elf: $(OBJS_7)
	@echo "Linking ARM7..."
	@$(CC) $(OBJS_7) $(LDFLAGS_7) -o $@

$(BUILD)/arm7/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling ARM7 $<"
	@$(CC) $(CFLAGS_7) -c $< -o $@

$(BUILD)/arm7/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling ARM7 $<"
	@$(CXX) $(CFLAGS_7) -fno-rtti -fno-exceptions -c $< -o $@

$(BUILD)/arm7/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "Assembling ARM7 $<"
	@$(CC) $(CFLAGS_7) -c $< -o $@
