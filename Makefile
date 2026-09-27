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


# 工具链（CROSS_PREFIX 由 mingw32/mingw64 目标传入）
CROSS_PREFIX=
MINGW32_PREFIX=i686-w64-mingw32-
MINGW64_PREFIX=x86_64-w64-mingw32-
EXEEXT=
CXX=$(CROSS_PREFIX)g++
RM=rm -f
CXFLAGS=-I. -finput-charset=UTF-8 -std=c++17 -O2 -DCZSC_BUILDING
LDFLAGS=
DLL_LDFLAGS=-static -static-libgcc -static-libstdc++ -Wl,--no-insert-timestamp
# 源码版本写入 adapter（czsc_build_commit）；工作区有未提交改动时加 -dirty。release 流程先 clean，确保重新写入
BUILD_COMMIT:=$(shell git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)$(shell git diff --quiet HEAD -- 2>/dev/null || echo -dirty)

# 目标：core/ 领域引擎，tdx/ 通达信适配层，tests/unit/ 单元测试（均为通配收录，新增文件无需改本文件）
BUILD_DIR=build
CHAN_OBJECTS=$(patsubst %.cpp,%.o,$(wildcard core/*.cpp))
TDX_OBJECTS=$(patsubst %.cpp,%.o,$(wildcard tdx/*.cpp))
API_OBJECTS=$(patsubst %.cpp,%.o,$(wildcard adapter/*.cpp))
DLL_OBJECTS=Main.o $(CHAN_OBJECTS) $(TDX_OBJECTS) $(API_OBJECTS)
DLL_TARGET=$(BUILD_DIR)/CZSC.dll
TEST_OBJECTS=$(CHAN_OBJECTS) $(TDX_OBJECTS) $(API_OBJECTS) $(patsubst %.cpp,%.o,$(wildcard tests/unit/*.cpp))
TEST_TARGET=tests/unit/ChanTests$(EXEEXT)
ALL_OBJECTS=$(sort $(DLL_OBJECTS) $(TEST_OBJECTS))
DEPENDS=$(ALL_OBJECTS:.o=.dep)

.PHONY: all mingw32 mingw64 check-mingw32 check-mingw64 mingw32-test-build mingw64-test-build \
        test test-build formula-test golden release release-check clean

all: $(DLL_TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# 静态链接 libgcc/libstdc++，并清零 PE 时间戳，避免无源码变化时 DLL 漂移
$(DLL_TARGET): $(DLL_OBJECTS) | $(BUILD_DIR)
	@echo [LD] $@
	@$(CXX) -shared -o $@ $^ $(DLL_LDFLAGS) $(LDFLAGS)

# 32 位通达信：build/CZSC.dll；64 位：build/CZSC64.dll（两版同源，仅指针宽度不同）
mingw32: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW32_PREFIX)
	@$(MAKE) clean

mingw64: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW64_PREFIX) DLL_TARGET=$(BUILD_DIR)/CZSC64.dll
	@$(MAKE) clean

check-mingw32:
	@command -v $(MINGW32_PREFIX)g++

check-mingw64:
	@command -v $(MINGW64_PREFIX)g++

mingw32-test-build: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW32_PREFIX) EXEEXT=.exe test-build
	@$(MAKE) clean

mingw64-test-build: clean
	@$(MAKE) CROSS_PREFIX=$(MINGW64_PREFIX) EXEEXT=.exe test-build
	@$(MAKE) clean

test: $(TEST_TARGET) formula-test
	@echo [TE] $(TEST_TARGET)
	@$(TEST_TARGET)

test-build: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS)
	@echo [LD] $@
	@$(CXX) -o $@ $^ $(LDFLAGS)

formula-test:
	@echo [TF] formulas
	@python3 tests/check_formulas.py

# 重新生成上证样本 golden（算法有意变更后运行，并人工核对 diff）
golden: $(TEST_TARGET)
	@CHAN_UPDATE_GOLDEN=1 $(TEST_TARGET) Golden

release:
	@sh scripts/build-release.sh

release-check:
	@sh scripts/check-release-dlls.sh

clean:
	@echo [RM] objects
	@$(RM) $(ALL_OBJECTS) $(DEPENDS) tests/unit/ChanTests tests/unit/ChanTests.exe

%.dep: %.cpp
	@$(CXX) $(CXFLAGS) -MM -MT $(@:.dep=.o) -o $@ $<

%.o: %.cpp
	@echo [CX] $<
	@$(CXX) $(CXFLAGS) -c -o $@ $<

adapter/%.o: adapter/%.cpp
	@echo [CX] $<
	@$(CXX) $(CXFLAGS) -DCZSC_BUILD_COMMIT='"$(BUILD_COMMIT)"' -c -o $@ $<

-include $(DEPENDS)
