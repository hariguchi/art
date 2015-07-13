CC       := gcc
PERL     := perl
PROF     := #-pg

# Flags
OPTFLAGS := -O0
#DEFS += -DNDEBUG
DEFS += -DDEBUG_FREE_HEAP
#CFLAGS    := -Wall -DPROFILING $(PROF) $(OPTFLAGS)
CFLAGS    := -Wall -g $(PROF) $(OPTFLAGS) $(DEFS)
LDFLAGS   := 

# Libraries
LDLIBS    := 
LOADLIBES := 


# Target names
TARGET    := rtLookup
LIBTARGET := libipart.a

# Directories to build
OBJDIR := obj/
DEPDIR := dep/


# Source files
LIBSRCS4 := 
SRCS4    := 
LIBSRCS6 := ipArt.c ipArtPathComp.c
SRCS6    := lkupTest.c #util.c
LIBSRCS  := $(LIBSRCS6)
SRCS     := $(SRCS6)

# Object files
LIBOBJS := $(addprefix $(OBJDIR),$(LIBSRCS:.c=.o))
OBJS    := $(addprefix $(OBJDIR),$(SRCS:.c=.o))


# Package files
PKGSRCS  := README Changelog Makefile Types.h util.c \
			ipArt.h ipArt.c ipArtPathComp.c lkupTest.c
PKGDATA  := ipa.txt rtTbl-random1.txt rtTbl-random2.txt rtTbl-random3.txt \
			v6ipa.txt v6routes-random1.txt v6routes-random2.txt
PKGSRCS  := $(foreach f,$(PKGSRCS),art/$(f))
PKGDATA  := $(foreach f,$(PKGDATA),art/$(f))
PKGFILES := $(PKGSRCS) $(PKGDATA)


$(TARGET): $(OBJS) $(LIBTARGET)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(PROF) -o $@

$(LIBTARGET): $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^
	ranlib $@

.PHONY: release
release:
	$(MAKE) DEFS=-DOPTIMIZATION_ON OPTFLAGS=-O3

.PHONY: test
test:
	$(MAKE) clean
	$(MAKE) release
	./tests.sh

.PHONY: prof
prof:
	$(MAKE) DEFS=-DOPTIMIZATION_ON OPTFLAGS=-O3 PROF=-pg

.PHONY: clean
clean:
	rm -f $(TARGET) $(LIBTARGET) $(OBJS) $(LIBOBJS) *.bak *~


# Include dependency files
include $(addprefix $(DEPDIR),$(SRCS:.c=.d))

$(DEPDIR)%.d : %.c
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< | sed "s@$*.o@& $@@g " > $@'

$(OBJDIR)%.o: %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o: %.cc
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

%.i : %.c
	$(CC) -E $(CPPFLAGS) $<
