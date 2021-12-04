# Makefile to build externals for Pure Data.
# Needs Makefile.pdlibbuilder as helper makefile for platform-dependent build
# settings and rules.

#PDDIR = "/Applications/Pd-0.51-4.app/Contents/Resources"

PYTHON_VER := $(shell python3 -c "import sys;v=sys.version_info;print(f'{v.major}.{v.minor}')")
PYTHON_CFLAGS  := `python3-config  --cflags`
PYTHON_LDFLAGS := `python3-config  --ldflags`
PYTHON_LIB=-lpython$(PYTHON_VER)

cflags += $(PYTHON_CFLAGS)
ldflags += $(PYTHON_LDFLAGS) $(PYTHON_LIB)

# library name
lib.name = pdpython

# input source file (class name == source file basename)
python.class.sources = pdpython.c

# all extra files to be included in binary distribution of the library
datafiles = python-help.pd python_help.py

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
PDLIBBUILDER_DIR=./pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder


display:
	@echo $(PYTHON_LIB)

format:
	@clang-format -style="{BasedOnStyle: Google, IndentWidth: 4}" -i pdpython.c
