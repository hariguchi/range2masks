CC         := gcc
TARGET     := range2masks
REV_MAJOR  := 0
REV_MINOR  := 1

OBJDIR := obj/
DEPDIR := dep/

ifeq ($(OS), Windows_NT)

DEFS       += -DOS_WINDOWS_NT
PROF       := 
GETLINESRC := getline.c
OWNER      := Administrator
GROUP      := Administrators
TGTGRP     := $(GROUP)
EXE        := .exe
PREFIX     := /usr

else

DEFS       += -DOS_UNIX
PROF       := 
GETLINESRC :=
OWNER      := root
GROUP      := root
TGTGRP     := $(TARGET)
EXE        :=
PREFIX     := /usr/local

endif

LDLIBS    :=
OPTFLAGS  := -g 
DEFS      += 
INCLUDES  := -I../include
CPPFLAGS  := $(DEFS) $(INCLUDES)
CFLAGS    := -Wall -Werror $(PROF) $(OPTFLAGS)
LOADLIBES := 

EXPT_INCL := 
SRCS      := range2masks.c
#SRCS      += 
OBJS      := $(addprefix $(OBJDIR),$(PTESTSRCS:.c=.o))
LIBOBJS   := 

BINDIR    := $(PREFIX)/bin
LIBDIR    := $(PREFIX)/lib
MANDIR    := $(PREFIX)/man


$(TARGET): $(OBJS)

.PHONY: prof
prof:
	$(MAKE) $(TARGET) PROF='-pg'


.PHONY: export
export:
	@ for l in $(EXPT_INCL) ; \
	do \
	  if [ -L ../include/$$l ] ; then \
	    rm -f ../include/$$l ; \
	  fi ; \
	  ln -s $(PWD)/$$l ../include ; \
	done
	@ if [ -L ../lib/$(TARGET) ] ; then \
	    rm -f ../lib/$(TARGET) ; \
	  fi ; \
	  ln -s $(PWD)/$(TARGET) ../lib

.PHONY: check
check:
	$(MAKE) -pq

.PHONY: package
package:
	@ mkdir package && mkdir package/$(TARGET) && \
	  mkdir package/$(TARGET)/dep && \
	  cp Makefile range2masks.c ../include/local-types.h \
	  package/$(TARGET) && cd package && \
	  tar cvf - . | bzip2 -9 > ../$(TARGET).tar.bz2 && \
	  cd .. && rm -rf package


include $(addprefix $(DEPDIR),$(SRCS:.c=.d))

$(DEPDIR)%.d : %.c
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< | sed "s@$*.o@$(OBJDIR)& $@@g " > $@'

clean:
	rm -f $(TARGET) $(DEPDIR)/*.d *.o *.bak *~ cscope.*
