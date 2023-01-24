###########################################################################
#
#   Filename:           Makefile
#
#   Author:             Marcelo Mourier
#   Created:            Tue Jan 24 09:48:48 MST 2023
#
#   Description:        This makefile is used to build the mkshiz
#                       command-line tool.
#
###########################################################################
#
#                  Copyright (c) 2023 Marcelo Mourier
#
###########################################################################

BIN_DIR = .
DEP_DIR = .
OBJ_DIR = .

OS := $(shell uname -o)

CFLAGS = -m64 -D_GNU_SOURCE -I. -ggdb -Wall -Werror -O0
LDFLAGS = -ggdb 

ifeq ($(OS),Cygwin)
	CFLAGS += -D__CYGWIN__
endif

SOURCES = $(wildcard *.c)
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(patsubst %.c,$(DEP_DIR)/%.d,$(SOURCES))

# Rule to autogenerate dependencies files
$(DEP_DIR)/%.d: %.c
	@set -e; $(RM) $@; \
         $(CC) -MM $(CPPFLAGS) $< > $@.temp; \
         sed 's,\($*\)\.o[ :]*,$(OBJ_DIR)\/\1.o $@ : ,g' < $@.temp > $@; \
         $(RM) $@.temp

# Rule to generate object files
$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

all: mkshiz

mkshiz: $(OBJECTS) Makefile
	$(CC) $(LDFLAGS) -o $(BIN_DIR)/$@ $(OBJECTS) -lcurl

clean:
	$(RM) $(OBJECTS) $(OBJ_DIR)/build_info.o $(DEP_DIR)/*.d $(BIN_DIR)/mkshiz

include $(DEPS)

