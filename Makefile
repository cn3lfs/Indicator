###############################################################################
# Visual K-Line Analysing System For Zen Theory
# Copyright (C) 2016, Martin Tang
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###############################################################################

# Configurations
CROSS_PREFIX=
MINGW32_PREFIX=i686-w64-mingw32-
MINGW64_PREFIX=x86_64-w64-mingw32-
EXEEXT=
CC=$(CROSS_PREFIX)gcc
CXX=$(CROSS_PREFIX)g++
AS=$(CROSS_PREFIX)as
FC=$(CROSS_PREFIX)g77
WINDRES=$(CROSS_PREFIX)windres
RM=rm -f
INCLUDE=
CHARSETFLAGS=-finput-charset=UTF-8
ASFLAGS=$(INCLUDE) -O2
CCFLAGS=$(INCLUDE) $(CHARSETFLAGS) -O2
CXFLAGS=$(INCLUDE) $(CHARSETFLAGS) -O2
FCFLAGS=$(INCLUDE) -O2
LDFLAGS=

# Objectives
BUILD_DIR=build
OBJECT1=Main.o CzscCore.o CzscAnalyzer.o
TARGET1=$(BUILD_DIR)/CZSC.dll
TEST_OBJECTS=CzscCore.o CzscAnalyzer.o tests/CzscCoreTests.o
TEST_TARGET=tests/CzscCoreTests$(EXEEXT)
TEST_TARGETS=tests/CzscCoreTests tests/CzscCoreTests.exe
SSE_DUMP_OBJECTS=CzscCore.o CzscAnalyzer.o tests/DumpSseResult.o
SSE_DUMP_TARGET=tests/dump_sse$(EXEEXT)
SSE_DUMP_TARGETS=tests/dump_sse tests/dump_sse.exe
LEGACY_OBJECTS=CCentroid.o
LEGACY_DEPENDS=CCentroid.dep
OBJECTS=$(OBJECT1)
TARGETS=$(TARGET1)
DEPENDS=$(OBJECTS:.o=.dep) $(TEST_OBJECTS:.o=.dep) $(SSE_DUMP_OBJECTS:.o=.dep)

# Build Commands
.PHONY: all mingw32 mingw32-test mingw32-test-build check-mingw32 \
        mingw64 mingw64-test mingw64-test-build check-mingw64 test test-build sse-result run clean debug

all : $(TARGETS)

mingw32: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW32_PREFIX)

mingw32-test:
	@$(MAKE) CROSS_PREFIX=$(MINGW32_PREFIX) test

check-mingw32:
	@command -v make
	@command -v $(MINGW32_PREFIX)gcc
	@command -v $(MINGW32_PREFIX)g++
	@command -v $(MINGW32_PREFIX)windres

# 64 位通达信版本：x86_64 工具链，产物 build/CZSC64.dll（指针随架构变 8 字节，pack/cdecl 不变）
mingw64: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW64_PREFIX) TARGET1=$(BUILD_DIR)/CZSC64.dll

mingw64-test:
	@$(MAKE) CROSS_PREFIX=$(MINGW64_PREFIX) test

check-mingw64:
	@command -v make
	@command -v $(MINGW64_PREFIX)gcc
	@command -v $(MINGW64_PREFIX)g++
	@command -v $(MINGW64_PREFIX)windres

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# 静态链接 libgcc/libstdc++(+winpthread)，使 DLL 自包含，免在通达信机器另装 MinGW 运行时
$(TARGET1) : $(OBJECTS) | $(BUILD_DIR)
	@echo [LD] $@
	@$(CXX) -shared -o $@ $^ -static -static-libgcc -static-libstdc++ $(LDFLAGS)

debug: all
	@echo [DB] $(TARGETS)
	@gdb -w $(TARGETS)

test: $(TEST_TARGET)
	@echo [TE] $(TEST_TARGET)
	@$(TEST_TARGET)

test-build: $(TEST_TARGET)

sse-result: clean $(SSE_DUMP_TARGET)
	@echo [SE] tests/czsc_sse_result.txt
	@$(SSE_DUMP_TARGET) tests/czsc_sse_result.txt

mingw32-test-build: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW32_PREFIX) EXEEXT=.exe test-build

mingw64-test-build: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW64_PREFIX) EXEEXT=.exe test-build

$(TEST_TARGET) : $(TEST_OBJECTS)
	@echo [LD] $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

$(SSE_DUMP_TARGET) : $(SSE_DUMP_OBJECTS)
	@echo [LD] $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

run: all
	@echo [EX] $(TARGETS)
	@$(TARGETS)

clean:
	@echo [RM] $(OBJECTS) $(TEST_OBJECTS) $(SSE_DUMP_OBJECTS) $(LEGACY_OBJECTS)
	@$(RM) $(DEPENDS) $(LEGACY_DEPENDS) $(OBJECTS) $(TEST_OBJECTS) $(SSE_DUMP_OBJECTS) $(LEGACY_OBJECTS) $(TEST_TARGETS) $(SSE_DUMP_TARGETS)

# Standard Procedures
%.dep : %.s
	@$(CC) $(INCLUDE) -MM -MT $(@:.dep=.o) -o $@ $<

%.dep : %.c
	@$(CC) $(CCFLAGS) -MM -MT $(@:.dep=.o) -o $@ $<

%.dep : %.m
	@$(CC) $(INCLUDE) -MM -MT $(@:.dep=.o) -o $@ $<

%.dep : %.cpp
	@$(CC) $(CXFLAGS) -MM -MT $(@:.dep=.o) -o $@ $<

%.dep : %.f
	@$(CC) $(INCLUDE) -MM -MT $(@:.dep=.o) -o $@ $<

%.dep : %.rc
	@$(CC) $(INCLUDE) -MM -MT $(@:.dep=.o) -o $@ $<

%.dep : %.l
	@$(CC) $(INCLUDE) -MM -MT $(@:.dep=.o) -o $@ $<

%.dep : %.y
	@$(CC) $(INCLUDE) -MM -MT $(@:.dep=.o) -o $@ $<

%.o : %.s
	@echo [AS] $<
	@$(AS) $(ASFLAGS) -o $@ $<

%.o : %.c
	@echo [CC] $<
	@$(CC) $(CCFLAGS) -c -o $@ $<

%.o : %.m
	@echo [OC] $<
	@$(CC) $(CCFLAGS) -c -o $@ $<

%.o : %.cpp
	@echo [CX] $<
	@$(CXX) $(CXFLAGS) -c -o $@ $<

%.o : %.f
	@echo [CX] $<
	@$(FC) $(FCFLAGS) -c -o $@ $<

%.o : %.rc
	@echo [CX] $<
	@$(WINDRES) $< $@

%.c : %.l
	@echo [FL] $<
	@flex -o $@ $<

%.c : %.y
	@echo [BS] $<
	@bison -d -o $@ $<

-include $(DEPENDS)
