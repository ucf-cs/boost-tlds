rstm_srcdir = $(top_srcdir)/src/rstm/rstm-dev
rstm_builddir = $(top_builddir)/src/rstm/rstm-compile

SUBDIRS = common/fraser boosting/skiplist

EXTRA_DIST = rstm/list/RSTMList.hpp

bin_PROGRAMS = trans runner

trans_SOURCES = bench/main.cc \
				common/allocator.cc\
				common/NodeAllocator.cc\
				common/NodeDescAllocator.cc\
				common/OnFinishAllocator.cc\
				common/timehelper.cc\
				common/assert.cc\
				ostm/skiplist/stmskip.cc\
				translink/transaction.cc\
				translink/list/translist.cc\
				translink/skiplist/transskip.cc\
				translink/map/listmap/transListMap.cc\
				translink/map/skipmap/transSkipMap.cc\
				boosting/list/boostinglist.cc\
				boosting/list/lockfreelist.cc\
				boosting/skiplist/boostingskip.cc\
				boosting/lockkey.cc

trans_CPPFLAGS = $(WARN_FLAGS) -std=c++11 -pthread\
				-DINTEL -DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE`\
				 -I$(top_builddir)/src -I$(top_srcdir)/src\
				 -I$(rstm_builddir)/include -I$(rstm_srcdir)/include -I$(rstm_srcdir)

trans_LDADD = $(BOOST_LDFLAGS) -L$(rstm_builddir)/libstm -lstm64 common/fraser/libfd.la boosting/skiplist/libbsl.la

EXTRA_runner_SOURCES  = bench/tests/TransListSuite.h

runner_SOURCES = bench/tests/runner-autogen.cpp\
				common/timehelper.cc\
				translink/transaction.cc\
				translink/list/translist.cc

BUILT_SOURCES = bench/tests/runner-autogen.cpp
MAINTAINERCLEANFILES = bench/tests/runner-autogen.cpp

bench/tests/runner-autogen.cpp: $(EXTRA_runner_SOURCES)
		/usr/bin/cxxtestgen --error-printer -o $@ $<