
uname = $(shell uname -s)

# Linux?
ifeq ($(uname),GNU/Linux)
CXXFLAGS += -D_LINUX -D_GNU_SOURCE
CFLAGS += -D_LINUX -D_GNU_SOURCE
endif
# Darwin? (Mac OS X)
ifeq ($(uname),Darwin)
CXXFLAGS += -D_DARWIN
CFLAGS += -D_DARWIN
endif

