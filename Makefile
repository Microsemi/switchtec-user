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
SYSCONFDIR ?= $(DESTDIR)/etc

CPPFLAGS=-Iinc -Ibuild -DCOMPLETE_ENV=\"SWITCHTEC_COMPLETE\"
CFLAGS=-g -O2 -fPIC -Wall
DEPFLAGS= -MT $@ -MMD -MP -MF $(OBJDIR)/$*.d
LDLIBS=-lcurses

LIB_SRCS=$(wildcard lib/*.c)
CLI_SRCS=$(wildcard cli/*.c)

LIB_OBJS=$(addprefix $(OBJDIR)/, $(patsubst %.c,%.o, $(LIB_SRCS)))
CLI_OBJS=$(addprefix $(OBJDIR)/, $(patsubst %.c,%.o, $(CLI_SRCS)))


ifneq ($(V), 1)
Q=@
else
NQ=:
endif

ifeq ($(W), 1)
CFLAGS += -Werror
endif

compile: libswitchtec.a libswitchtec.so switchtec

clean:
	$(Q)rm -rf libswitchtec.a libswitchtec.so switchtec build

$(OBJDIR)/version.h $(OBJDIR)/version.mk: FORCE $(OBJDIR)
	@$(SHELL_PATH) ./VERSION-GEN
$(OBJDIR)/cli/main.o: $(OBJDIR)/version.h
-include $(OBJDIR)/version.mk

$(OBJDIR):
	$(Q)mkdir -p $(OBJDIR)/cli $(OBJDIR)/lib

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	@$(NQ) echo "  CC    $<"
	$(Q)$(COMPILE.c) $(DEPFLAGS) $< -o $@

libswitchtec.a: $(LIB_OBJS)
	@$(NQ) echo "  AR    $@"
	$(Q)$(AR) rDsc $@ $^

libswitchtec.so: $(LIB_OBJS)
	@$(NQ) echo "  LD    $@"
	$(Q)$(LINK.o) -shared $^ -o $@

switchtec: $(CLI_OBJS) libswitchtec.a
	@$(NQ) echo "  LD    $@"
	$(Q)$(LINK.o) $^ $(LDLIBS) -o $@

install-bash-completion:
	@$(NQ) echo "  INSTALL  $(SYSCONFDIR)/bash_completion.d/bash-switchtec-completion.sh"
	$(Q)install -d $(SYSCONFDIR)/bash_completion.d
	$(Q)install -m 644 -T ./completions/bash-switchtec-completion.sh \
		$(SYSCONFDIR)/bash_completion.d/switchtec

install-bin: compile
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

install: install-bin install-bash-completion

uninstall:
	@$(NQ) echo "  UNINSTALL  $(BINDIR)/switchtec"
	$(Q)rm -f $(BINDIR)/switchtec
	@$(NQ) echo "  UNINSTALL  $(LIBDIR)/libswitchtec.a"
	$(Q)rm -f $(LIBDIR)/libswitchtec.a
	@$(NQ) echo "  UNINSTALL  $(LIBDIR)/libswitchtec.so"
	$(Q)rm -f $(LIBDIR)/libswitchtec.so*
	@$(NQ) echo "  LDCONFIG"
	$(Q)ldconfig

PKG=switchtec-$(FULL_VERSION)
dist:
	git archive --format=tar --prefix=$(PKG)/ HEAD > $(PKG).tar
	@echo $(FULL_VERSION) > version
	tar -rf $(PKG).tar --xform="s%^%$(PKG)/%" version
	xz -f $(PKG).tar
	rm -f version

.PHONY: clean compile install unintsall install-bin install-bash-completion
.PHONY: FORCE


-include $(patsubst %.o,%.d,$(LIB_OBJS))
