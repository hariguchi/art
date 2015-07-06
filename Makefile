CC       := gcc
PERL     := perl
PROF     := #-pg
LDLIBS   := #-lxnet
SRCHTEST := -DSEARCH_TEST
OPTFLAGS := -g -O0 -Dinline='' $(TBLDEF)
#OPTFLAGS := -g -O6 $(TBLDEF)
#DEFS += -Dinline=''
#DEFS += -DNDEBUG
DEFS += -DDEBUG_FREE_HEAP


# make an image to do simple lookup if invoked as `make simple=1'
#CFLAGS   := -Wall -DPROFILING $(PROF) $(OPTFLAGS)
CFLAGS   := -Wall $(PROF) $(OPTFLAGS) $(SRCHTEST) $(DEFS)
LOADLIBES := 
OBJDIR := .


# Target name
TARGET := rtLookup

# Directories to build
OBJDIR := obj/
DEPDIR := dep/


# Source files
SRCS4 := ipArt4.c lkupTest4.c util.c
SRCS6 := ipArt.c ipArtPathComp.c lkupTest.c util.c
SRCS := $(SRCS6)

# Object files
OBJS   := $(addprefix $(OBJDIR),$(SRCS:.c=.o))


# Package files
PKGSRCS  := README Changelog Makefile Types.h util.c \
			ipArt.h ipArt.c ipArtPathComp.c lkupTest.c \
			ipArt4.h ipArt4.c lkupTest4.c
PKGDATA  := ipa.txt rtTbl-random1.txt rtTbl-random2.txt rtTbl-random3.txt \
			v6ipa.txt v6routes-random1.txt v6routes-random2.txt \
			mkHist.pl mkStat.pl
PKGSRCS  := $(foreach f,$(PKGSRCS),art/$(f))
PKGDATA  := $(foreach f,$(PKGDATA),art/$(f))
PKGFILES := $(PKGSRCS) $(PKGDATA)


$(TARGET): $(OBJS)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) $(PROF) -o $@



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



.PHONY: v6
v6:
	$(MAKE) IPVER=6

.PHONY: v6clean
v6clean:
	$(MAKE) doClean IPVER=6


.PHONY: perf
perf:
	./$(TARGET) $(ARGS) && gprof $(TARGET) gmon.out

.PHONY: perf4
perf4:
	@ if [ -z "$(ARGS)" ]; then \
		$(MAKE) perf ARGS='4 batch'; \
	else \
		$(MAKE) perf ARGS='4 $(ARGS)'; \
	fi

.PHONY: perf6
perf6:
	@ if [ -z "$(ARGS)" ]; then \
		$(MAKE) perf ARGS='6 batch'; \
	else \
		$(MAKE) perf ARGS='6 $(ARGS)'; \
	fi


.PHONY: stats
stats:
	@ rm -f st*; \
	i='0'; \
	while [ $$i -lt 100 ]; \
	do \
		if [ `expr $$i % 5` -eq 0 ]; then \
			echo $$i; \
		fi; \
		./$(TARGET) $(ARGS) || exit $?; \
		gprof $(TARGET) gmon.out | perl mkStat.pl; \
		i=`expr $$i + 1`; \
	done; \
	$(MAKE) showstats

.PHONY: stats4
stats4:
	@ if [ -z "$(ARGS)" ]; then \
		$(MAKE) stats ARGS='4 batch'; \
	else \
		$(MAKE) stats ARGS='4 $(ARGS)'; \
	fi


.PHONY: showstats
showstats:
	@ echo "add:";        $(PERL) mkHist.pl stat-add.txt; \
	echo; echo "delete:"; $(PERL) mkHist.pl stat-del.txt; \
	echo; echo "search:"; $(PERL) mkHist.pl stat-search.txt; 

.PHONY: clean
clean:
	$(MAKE) doClean
	$(MAKE) doClean IPVER=6

.PHONY: doClean
doClean:
	rm -f $(TARGET) $(OBJS) *.bak *~

.PHONY: tar
tar:
	@ cd ..; \
	tar cvf - $(PKGFILES) | bzip2 -9 > art.tar.bz2 && \
	ls -l art.tar.bz2

.PHONY: tar-src
tar-src:
	@ cd ..; \
	tar cvf - $(PKGSRCS) | bzip2 -9 > art-src.tar.bz2 && \
	ls -l art-src.tar.bz2

.PHONY: tar-data
tar-data:
	@ cd ..; \
	tar cvf - $(PKGDATA) | bzip2 -9 > art-data.tar.bz2 && \
	ls -l art-data.tar.bz2
