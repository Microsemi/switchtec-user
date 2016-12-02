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

version.h: FORCE
	@$(SHELL_PATH) ./VERSION-GEN
$(OBJDIR)/cli/main.o: version.h

$(OBJDIR):
	$(Q)mkdir -p $(OBJDIR)/cli $(OBJDIR)/lib

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	@$(NQ) echo "  CC    $<" $(QQ)
	$(Q)$(COMPILE.c) $(DEPFLAGS) $< -o $@

libswitchtec.a: $(LIB_OBJS)
	@$(NQ) echo "  AR    $@" $(QQ)
	$(Q)$(AR) rusc $@ $^

libswitchtec.so: $(LIB_OBJS)
	@$(NQ) echo "  LD    $@" $(QQ)
	$(Q)$(LINK.o) -shared $^ -o $@

switchtec: $(CLI_OBJS) libswitchtec.a
	@$(NQ) echo "  LD    $@" $(QQ)
	$(Q)$(LINK.o) $^ -o $@

clean:
	$(Q)rm -rf libswitchtec.a libswitchtec.so switchtec build version.h

.PHONY: clean compile FORCE


-include $(patsubst %.o,%.d,$(LIB_OBJS))
