CC       := gcc
PERL     := perl
PROF     := #-pg

# Flags
OPTFLAGS := -O0
DEFS     += -DDEBUG_FREE_HEAP
CFLAGS   := -Wall -g $(PROF) $(OPTFLAGS) $(DEFS)
LDFLAGS  := 

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



$(TARGET): $(OBJS) $(LIBTARGET)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(PROF) -o $@

$(LIBTARGET): $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $^
	ranlib $@

.PHONY: release
release:
	$(MAKE) DEFS='-DOPTIMIZATION_ON -DNDEBUG' OPTFLAGS=-O3

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

$(OBJDIR)%.o: %.c $(DEPDIR)%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o: %.cc $(DEPDIR)%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o: %.cpp $(DEPDIR)%.d
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

%.i : %.c
	$(CC) -E $(CPPFLAGS) $<
