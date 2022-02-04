# Makefile, v1
# Sistemas Operativos, DEI/IST/ULisboa 2021-22
#
# This makefile should be run from the *root* of the project

CC ?= gcc
LD ?= gcc

# space separated list of directories with header files
INCLUDE_DIRS := fs common client .
# this creates a space separated list of -I<dir> where <dir> is each of the values in INCLUDE_DIRS
INCLUDES = $(addprefix -I, $(INCLUDE_DIRS))

SOURCES  := $(wildcard */*.c)
HEADERS  := $(wildcard */*.h)
OBJECTS  := $(SOURCES:.c=.o)
TARGET_EXECS := fs/tfs_server tests/lib_destroy_after_all_closed_test tests/lib_destroy_after_all_closed_test_1 tests/lib_destroy_after_all_closed_test_2 tests/client_server_simple_test tests/client_server_error_1 tests/2_clients_same_protocol tests/2_clients_different_protocol tests/5_clients_different_protocol tests/2_clients_1_shutdown tests/more_clients_than_supported tests/shutdown_after_too_many_sessions_error_test

# VPATH is a variable used by Makefile which finds *sources* and makes them available throughout the codebase
# vpath %.h <DIR> tells make to look for header files in <DIR>
vpath # clears VPATH
vpath %.h $(INCLUDE_DIRS)

CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L 
CFLAGS += $(INCLUDES)

# Warnings
CFLAGS += -fdiagnostics-color=always -lpthread -pthread -Wall -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused
# Warning suppressions
CFLAGS += -Wno-sign-compare

# optional debug symbols: run make DEBUG=no to deactivate them
ifneq ($(strip $(DEBUG)), no)
  CFLAGS += -g
endif

# optional O3 optimization symbols: run make OPTIM=no to deactivate them
ifeq ($(strip $(OPTIM)), no)
  CFLAGS += -O0
else
  CFLAGS += -O3
endif

LDFLAGS = -pthread

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean depend fmt

all: clean $(TARGET_EXECS)

test: test0 test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11

test0:
	@printf "\n"
	@echo ------- lib_destroy_after_all_closed_test ------------
	./tests/lib_destroy_after_all_closed_test

test1:
	@printf "\n"
	@echo ------- lib_destroy_after_all_closed_test_1 ------------
	./tests/lib_destroy_after_all_closed_test_1

test2:
	@printf "\n"
	@echo ------- lib_destroy_after_all_closed_test_2 ------------
	./tests/lib_destroy_after_all_closed_test_2

test3:
	@printf "\n"
	@echo ------- client_server_simple_test ------------
	./tests/client_server_simple_test /tmp/cli /tmp/server

test4:
	@printf "\n"
	@echo ------- client_server_error_1 ------------
	./tests/client_server_error_1 /tmp/cli /tmp/server

test5:
	@printf "\n"
	@echo ------- client_simple_test_fork ------------
	./tests/client_simple_test_fork /tmp/cli /tmp/server

test6:
	@printf "\n"
	@echo ------- 2_clients_same_protocol ------------
	./tests/2_clients_same_protocol /tmp/cli1 /tmp/cli2 /tmp/server

test7:
	@printf "\n"
	@echo ------- 2_clients_different_protocol ------------
	./tests/2_clients_different_protocol /tmp/cli1 /tmp/cli2 /tmp/server

test8:
	@printf "\n"
	@echo ------- 5_clients_different_protocol ------------
	./tests/5_clients_different_protocol /tmp/cli1 /tmp/cli2 /tmp/cli3 /tmp/cli4 /tmp/cli5 /tmp/server

test9:
	@printf "\n"
	@echo ------- more_clients_than_supported ------------
	./tests/more_clients_than_supported /tmp/cli1 /tmp/cli2 /tmp/cli3 /tmp/cli4 /tmp/cli5 /tmp/server

test10:
	@printf "\n"
	@echo ------- 2_clients_1_shutdown ------------
	./tests/2_clients_1_shutdown /tmp/cli1 /tmp/cli2 /tmp/server

test11:
	@printf "\n"
	@echo ------- shutdown_after_too_many_sessions_error_test ------------
	./tests/shutdown_after_too_many_sessions_error_test /tmp/cli1 /tmp/cli2 /tmp/cli3 /tmp/cli4 /tmp/server

# The following target can be used to invoke clang-format on all the source and header
# files. clang-format is a tool to format the source code based on the style specified 
# in the file '.clang-format'.
# More info available here: https://clang.llvm.org/docs/ClangFormat.html

# The $^ keyword is used in Makefile to refer to the right part of the ":" in the 
# enclosing rule. See https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

fmt: $(SOURCES) $(HEADERS)
	clang-format -i $^


# Note the lack of a rule.
# make uses a set of default rules, one of which compiles C binaries
# the CC, LD, CFLAGS and LDFLAGS are used in this rule
tests/client_server_simple_test: tests/client_server_simple_test.o client/tecnicofs_client_api.o common/common.o
tests/2_clients_same_protocol: tests/2_clients_same_protocol.o client/tecnicofs_client_api.o common/common.o
tests/2_clients_1_shutdown: tests/2_clients_1_shutdown.o client/tecnicofs_client_api.o common/common.o
tests/more_clients_than_supported: tests/more_clients_than_supported.o client/tecnicofs_client_api.o common/common.o
tests/2_clients_different_protocol: tests/2_clients_different_protocol.o client/tecnicofs_client_api.o common/common.o
tests/5_clients_different_protocol: tests/5_clients_different_protocol.o client/tecnicofs_client_api.o common/common.o
tests/client_server_error_1: tests/client_server_error_1.o client/tecnicofs_client_api.o common/common.o
fs/tfs_server: fs/operations.o fs/state.o common/common.o
tests/lib_destroy_after_all_closed_test: tests/lib_destroy_after_all_closed_test.o fs/operations.o fs/state.o common/common.o
tests/lib_destroy_after_all_closed_test_1: tests/lib_destroy_after_all_closed_test_1.o fs/operations.o fs/state.o common/common.o
tests/lib_destroy_after_all_closed_test_2: tests/lib_destroy_after_all_closed_test_2.o fs/operations.o fs/state.o common/common.o
tests/shutdown_after_too_many_sessions_error_test: tests/shutdown_after_too_many_sessions_error_test.o client/tecnicofs_client_api.o common/common.o

clean:
	rm -f $(OBJECTS) $(TARGET_EXECS)


# This generates a dependency file, with some default dependencies gathered from the include tree
# The dependencies are gathered in the file autodep. You can find an example illustrating this GCC feature, without Makefile, at this URL: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/GCC/options/MM
# Run `make depend` whenever you add new includes in your files
depend : $(SOURCES)
	$(CC) $(INCLUDES) -MM $^ > autodep