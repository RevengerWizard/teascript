CC = gcc
CFLAGS = #-Wall
MODE = debug
RM = del

ifeq ($(MODE), debug)
    CFLAGS += -g -O0
else
    CFLAGS += -O2 -s
endif

SRC_FILES = $(wildcard *.c)
OBJ_FILES = $(patsubst %.c, %.o, $(SRC_FILES))

#-DTEA_DEBUG
#-DTEA_DEBUG_TOKENS
#-DTEA_DEBUG_PRINT_CODE
#-DTEA_DEBUG_TRACE_EXECUTION
#-DTEA_DEBUG_TRACE_MEMORY
#-DTEA_DEBUG_FINAL_MEMORY
#-DTEA_DEBUG_STRESS_GC
#-DTEA_DEBUG_LOG_GC
MACRO =  -DTEA_DEBUG -DTEA_DEBUG_FINAL_MEMORY #-DTEA_DEBUG_TRACE_EXECUTION
CFLAGS += $(MACRO)

tea: $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean
all: tea

clean:
	@$(RM) $(OBJ_FILES) tea.exe