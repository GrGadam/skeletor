# ============================================================================
#   SF2000 SKELETOR LIBRETRO MAKEFILE
# ============================================================================

STATIC_LINKING := 0
AR             := ar

ifneq ($(V),1)
   Q := @
endif

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) $(CFLAGS)
   CXXFLAGS := -fsanitize=$(SANITIZER) $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# =============================================================================
# PLATFORM DETECTION
# =============================================================================

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

# System platform
system_platform = unix
ifeq ($(shell uname -a),)
	EXE_EXT = .exe
	system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	system_platform = osx
	arch = intel
ifeq ($(shell uname -p),powerpc)
	arch = ppc
endif
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	system_platform = win
endif

# =============================================================================
# CORE SETTINGS
# =============================================================================

CORE_DIR     := .
TARGET_NAME  := skeletor
LIBM         := -lm

ifeq ($(ARCHFLAGS),)
ifeq ($(archs),ppc)
   ARCHFLAGS = -arch ppc -arch ppc64
else
   ARCHFLAGS = -arch i386 -arch x86_64
endif
endif

ifeq ($(platform), osx)
ifndef ($(NOUNIVERSAL))
   CXXFLAGS += $(ARCHFLAGS)
   LFLAGS += $(ARCHFLAGS)
endif
endif

ifeq ($(STATIC_LINKING), 1)
EXT := a
endif

# =============================================================================
# PLATFORM RULES
# =============================================================================

# ------------------ UNIX ---------------------
ifeq ($(platform), unix)
	EXT ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined

# ------------------ LINUX PORTABLE -----------
else ifeq ($(platform), linux-portable)
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC -nostdlib
	SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T
	LIBM :=

# ------------------ OSX ----------------------
else ifneq (,$(findstring osx,$(platform)))
	TARGET := $(TARGET_NAME)_libretro.dylib
	fpic := -fPIC
	SHARED := -dynamiclib

# ------------------ iOS ----------------------
else ifneq (,$(findstring ios,$(platform)))
	TARGET := $(TARGET_NAME)_libretro_ios.dylib
	fpic := -fPIC
	SHARED := -dynamiclib

# ------------------ QNX ----------------------
else ifneq (,$(findstring qnx,$(platform)))
	TARGET := $(TARGET_NAME)_libretro_qnx.so
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined

# ------------------ EMCC ---------------------
else ifeq ($(platform), emscripten)
	TARGET := $(TARGET_NAME)_libretro_emscripten.bc
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined

# =============================================================================
# SF2000 (MIPS32 + static .a output)
# =============================================================================
else ifeq ($(platform), sf2000)

	TARGET := $(TARGET_NAME)_libretro_$(platform).a

	MIPS := /opt/mips32-mti-elf/2019.09-03-2/bin/mips-mti-elf-
	CC   = $(MIPS)gcc
	CXX  = $(MIPS)g++
	AR   = $(MIPS)ar

	CFLAGS = -EL -march=mips32r2 -mtune=mips32r2 -msoft-float -G0 -mno-abicalls -fno-pic
	CFLAGS += -Os
	CFLAGS += -ffast-math -fomit-frame-pointer -ffunction-sections -fdata-sections
	CFLAGS += -DROM_BUFFER_SIZE=16
	CFLAGS += -DSF2000

	CXXFLAGS = $(CFLAGS)

	STATIC_LINKING = 1
	HAVE_DYNAREC := 0
	CPU_ARCH := mips

# ------------------ DEFAULT WIN ----------------
else
	CC = gcc
	TARGET := $(TARGET_NAME)_libretro.dll
	SHARED := -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=$(CORE_DIR)/link.T -Wl,--no-undefined

endif

# =============================================================================
# COMPILATION FLAGS
# =============================================================================

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g -DDEBUG
   CXXFLAGS += -O0 -g -DDEBUG
else
   CFLAGS += -O3
   CXXFLAGS += -O3
endif

include Makefile.common

OBJECTS := $(SOURCES_C:.c=.o) $(SOURCES_CXX:.cpp=.o)

CFLAGS   += -Wall -D__LIBRETRO__ $(fpic)
CXXFLAGS += -Wall -D__LIBRETRO__ $(fpic)

# =============================================================================
# BUILD RULES
# =============================================================================

all: $(TARGET)

$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(CXX) $(fpic) $(SHARED) $(INCLUDES) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBM)
endif

%.o: %.c
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(fpic) -c -o $@ $<

%.o: %.cpp
	@$(if $(Q), $(shell echo echo CXX $<),)
	$(Q)$(CXX) $(CXXFLAGS) $(fpic) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean
