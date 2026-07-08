# ============================================================
#  Makefile — Esteira Separadora CLI
#  Alvo: Raspberry Pi 3B (Linux ARMv7 / ARM64)
#
#  Dependencias:
#    sudo apt install libopencv-dev libzbar-dev libgpiod-dev
#
#  Uso:
#    make          — compila
#    make clean    — remove binarios
#    make run      — compila e executa com configuracoes padrao
#    make run-fsm  — executa direto no modo FSM
#    make run-dbg  — executa direto no modo DEBUG
#    make run-hmi  — executa direto no modo HMI
#    make docs     — gera a documentacao Doxygen em docs/html
#
#  Modo HMI usa 3 linhas GPIO (ver hmi.h):
#    GPIO17 — botao START     GPIO27 — botao STOP     GPIO23 — buzzer
# ============================================================

CC      = gcc
CXX     = g++
TARGET  = esteira

CFLAGS  = -Wall -Wextra -std=c99 -O2 -g
CXXFLAGS = -Wall -Wextra -std=c++11 -O2 -g

OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4 2>/dev/null || \
                          pkg-config --cflags opencv  2>/dev/null)
OPENCV_LIBS   := $(shell pkg-config --libs   opencv4 2>/dev/null || \
                          pkg-config --libs   opencv  2>/dev/null)

ZBAR_CFLAGS   := $(shell pkg-config --cflags zbar 2>/dev/null)
ZBAR_LIBS     := $(shell pkg-config --libs   zbar 2>/dev/null || echo "-lzbar")

GPIOD_CFLAGS  := $(shell pkg-config --cflags libgpiod 2>/dev/null)
GPIOD_LIBS    := $(shell pkg-config --libs   libgpiod 2>/dev/null || echo "-lgpiod")

PTHREAD_LIBS  = -lpthread

CFLAGS   += $(OPENCV_CFLAGS) $(ZBAR_CFLAGS) $(GPIOD_CFLAGS)
CXXFLAGS += $(OPENCV_CFLAGS) $(ZBAR_CFLAGS) $(GPIOD_CFLAGS)
LDFLAGS  = $(OPENCV_LIBS) $(ZBAR_LIBS) $(GPIOD_LIBS) $(PTHREAD_LIBS)

SRCS = main.c         \
       serial.c       \
       protocol.c     \
       fsm.c          \
       debug.c        \
       hmi.c          \
       lcd2004_i2c.c  \
       gpio_button.c  \
       gpio_buzzer.c  \
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
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: all
	./$(TARGET) -p /dev/ttyS0 -b 115200 -c 0

run-fsm: all
	./$(TARGET) -p /dev/ttyS0 -b 115200 -c 0 -m 1

run-dbg: all
	./$(TARGET) -p /dev/ttyS0 -b 115200 -c 0 -m 2

run-hmi: all
	./$(TARGET) -p /dev/ttyS0 -b 115200 -c 0 -m 3

docs:
	doxygen Doxyfile
	@echo "Documentacao gerada em docs/html/index.html"

docs-clean:
	rm -rf docs

.PHONY: all clean run run-fsm run-dbg run-hmi docs docs-clean
