CC = /usr/bin/gcc
CFLAGS = -Wall -Wextra    # 启用常用的以及额外的警告信息
SOURCES = $(wildcard *.c)    # 当前目录下所有 .c 结尾的文件
EXECUTABLES = $(patsubst %.c,%,$(SOURCES))    # 将 *.c 去掉

all: $(EXECUTABLES)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXECUTABLES)

.PHONY: all clean