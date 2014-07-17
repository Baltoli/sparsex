#
# Makefile.am -- SparseX library
#
# Copyright (C) 2014, Computing Systems Laboratory (CSLab), NTUA
# Copyright (C) 2014, Vasileios Karakasis
# All rights reserved.
#
# This file is distributed under the BSD License. See LICENSE.txt for details.
#

AM_DEFAULT_VERBOSITY = 0
ACLOCAL_AMFLAGS = -I m4check

AM_CPPFLAGS = \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include \
	$(AX_CPPFLAGS)

AM_CFLAGS = $(AX_CFLAGS)
AM_CXXFLAGS = $(AX_CXXFLAGS)
AM_LDFLAGS  = $(AX_LDFLAGS)

lib_LTLIBRARIES = libsparseximpl.la
libsparseximpl_la_SOURCES = \
	numa_util.c \
	Affinity.cpp \
	Barrier.cpp \
	Compiler.cpp \
	CsxBuild.cpp \
	CsxKernels.cpp \
	CsxSpmv.cpp \
	CsxUtil.cpp \
	CtlBuilder.cpp \
	Encodings.cpp \
	Facade.cpp \
	Jit.cpp \
	JitUtil.cpp \
	Mmf.cpp \
	Node.cpp \
	Runtime.cpp \
	Statistics.cpp \
	TemplateText.cpp \
	ThreadPool.cpp \
	Timer.cpp \
	TimerCollection.cpp \
	Vector.cpp \
	logger/Logger.cpp \
	logger/OutputPolicy.cpp

libsparseximpl_la_LIBADD = \
	$(BOOST_REGEX_LIB) \
	$(BOOST_SERIALIZATION_LIB) \
	$(BOOST_SYSTEM_LIB) \
	$(BOOST_THREAD_LIB) \
	$(CLANG_LIBS) \
	$(LLVM_LIBS)