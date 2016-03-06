# Copyright (C) 2015 The PASTA Team
# Copyright (C) 2016 173210 <root.3.173210@live.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

.PHONY: all release clean

export COMMON := ../common.mk

TARGET := $(addprefix ../$(BUILD)/,ktr/native_firm	\
	ctr/native_firm ctr/agb_firm ctr/twl_firm)

all: $(TARGET)

release: $(RELEASE)/r3tools/patch

../$(BUILD)/ktr/%: %
	$(MAKE) BUILD=$@ PLATFORM_KTR=1 -C $<

../$(BUILD)/ctr/%: %
	$(MAKE) BUILD=$@ -C $<

$(RELEASE)/r3tools/patch: $(TARGET)
	mkdir -p $@
	cp $(BUILD)/ktr/native_firm/native_firm.elf $@/0004013820000002.elf
	cp $(BUILD)/ctr/native_firm/native_firm.elf $@/0004013800000002.elf
	cp $(BUILD)/ctr/agb_firm/agb_firm.elf $@/0004013800000202.elf
	cp $(BUILD)/ctr/twl_firm/twl_firm.elf $@/0004013800000102.elf

clean:
	rm -Rf build
