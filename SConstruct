env = Environment(CCFLAGS = "-DFUSE_USE_VERSION=26 -D_FILE_OFFSET_BITS=64 -O2 -Wall -Wextra -std=gnu99")

viewfs = env.Program("viewfs", Glob("src/*.c"), LIBS = ["fuse", "attr"])
