########################################################################
##
## Microsemi Switchtec(tm) PCIe Management Library
## Copyright (c) 2016, Microsemi Corporation
##
## This program is free software; you can redistribute it and/or modify it
## under the terms and conditions of the GNU General Public License,
## version 2, as published by the Free Software Foundation.
##
## This program is distributed in the hope it will be useful, but WITHOUT
## ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
## FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
## more details.
##
########################################################################

OBJDIR=build

DESTDIR ?=
PREFIX ?= /usr/local

BINDIR ?= $(DESTDIR)$(PREFIX)/bin
LIBDIR ?= $(DESTDIR)$(PREFIX)/lib

CPPFLAGS=-Iinc -I.
CFLAGS=-g -O2 -fPIC -Wall
DEPFLAGS= -MT $@ -MMD -MP -MF $(OBJDIR)/$*.d

LIB_SRCS=$(wildcard lib/*.c)
CLI_SRCS=$(wildcard cli/*.c)

LIB_OBJS=$(addprefix $(OBJDIR)/, $(patsubst %.c,%.o, $(LIB_SRCS)))
CLI_OBJS=$(addprefix $(OBJDIR)/, $(patsubst %.c,%.o, $(CLI_SRCS)))


ifneq ($(V), 1)
Q=@
else
NQ=:
endif

compile: libswitchtec.a libswitchtec.so switchtec

version.h version.mk: FORCE
	@$(SHELL_PATH) ./VERSION-GEN
$(OBJDIR)/cli/main.o: version.h
-include version.mk

$(OBJDIR):
	$(Q)mkdir -p $(OBJDIR)/cli $(OBJDIR)/lib

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	@$(NQ) echo "  CC    $<"
	$(Q)$(COMPILE.c) $(DEPFLAGS) $< -o $@

libswitchtec.a: $(LIB_OBJS)
	@$(NQ) echo "  AR    $@"
	$(Q)$(AR) rusc $@ $^

libswitchtec.so: $(LIB_OBJS)
	@$(NQ) echo "  LD    $@"
	$(Q)$(LINK.o) -shared $^ -o $@

switchtec: $(CLI_OBJS) libswitchtec.a
	@$(NQ) echo "  LD    $@"
	$(Q)$(LINK.o) $^ -o $@

clean:
	$(Q)rm -rf libswitchtec.a libswitchtec.so switchtec build version.h

install: compile
	$(Q)install -d $(BINDIR) $(LIBDIR)

	@$(NQ) echo "  INSTALL  $(BINDIR)/switchtec"
	$(Q)install -s switchtec $(BINDIR)
	@$(NQ) echo "  INSTALL  $(LIBDIR)/libswitchtec.a"
	$(Q)install -m 0664 libswitchtec.a $(LIBDIR)
	@$(NQ) echo "  INSTALL  $(LIBDIR)/libswitchtec.so.$(VERSION)"
	$(Q)install libswitchtec.so $(LIBDIR)/libswitchtec.so.$(VERSION)
	@$(NQ) echo "  INSTALL  $(LIBDIR)/libswitchtec.so"
	$(Q)ln -fs $(LIBDIR)/libswitchtec.so.$(VERSION) \
           $(LIBDIR)/libswitchtec.so

	@$(NQ) echo "  LDCONFIG"
	$(Q)ldconfig

uninstall:
	@$(NQ) echo "  UNINSTALL  $(BINDIR)/switchtec"
	$(Q)rm -f $(BINDIR)/switchtec
	@$(NQ) echo "  UNINSTALL  $(LIBDIR)/libswitchtec.a"
	$(Q)rm -f $(LIBDIR)/libswitchtec.a
	@$(NQ) echo "  UNINSTALL  $(LIBDIR)/libswitchtec.so"
	$(Q)rm -f $(LIBDIR)/libswitchtec.so*
	@$(NQ) echo "  LDCONFIG"
	$(Q)ldconfig

.PHONY: clean compile install unintsall FORCE


-include $(patsubst %.o,%.d,$(LIB_OBJS))
