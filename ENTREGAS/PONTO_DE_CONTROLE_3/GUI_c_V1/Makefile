# ============================================================
#  Makefile — Esteira Separadora CLI
#  Alvo: Raspberry Pi 3B (Linux ARMv7 / ARM64)
#
#  Dependencias:
#    sudo apt install libopencv-dev libzbar-dev
#
#  Uso:
#    make          — compila
#    make clean    — remove binarios
#    make run      — compila e executa com configuracoes padrao
#    make run-fsm  — executa direto no modo FSM
#    make run-dbg  — executa direto no modo DEBUG
# ============================================================

CC      = gcc
CXX     = g++
TARGET  = esteira

CFLAGS  = -Wall -Wextra -std=c99 -O2 -g

OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4 2>/dev/null || \
                          pkg-config --cflags opencv  2>/dev/null)
OPENCV_LIBS   := $(shell pkg-config --libs   opencv4 2>/dev/null || \
                          pkg-config --libs   opencv  2>/dev/null)

ZBAR_CFLAGS   := $(shell pkg-config --cflags zbar 2>/dev/null)
ZBAR_LIBS     := $(shell pkg-config --libs   zbar 2>/dev/null || echo "-lzbar")

PTHREAD_LIBS  = -lpthread

CFLAGS  += $(OPENCV_CFLAGS) $(ZBAR_CFLAGS)
LDFLAGS  = $(OPENCV_LIBS) $(ZBAR_LIBS) $(PTHREAD_LIBS)

SRCS = main.c     \
       serial.c   \
       protocol.c \
       fsm.c      \
       debug.c    \
       log.c

SRCS_CXX = camera.cpp

OBJS = $(SRCS:.c=.o) $(SRCS_CXX:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build concluido: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CFLAGS) -std=c++11 -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: all
	./$(TARGET) -p /dev/ttyS0 -b 115200 -c 0

run-fsm: all
	./$(TARGET) -p /dev/ttyS0 -b 115200 -c 0 -m 1

run-dbg: all
	./$(TARGET) -p /dev/ttyS0 -b 115200 -c 0 -m 2

.PHONY: all clean run run-fsm run-dbg
