SRC_DIR = ./src
OBJ_DIR = ./obj
BIN_DIR = ./bin
INC_DIR = ./include

SRCS = $(wildcard $(SRC_DIR)/*.c)
HEADERS = $(wildcard $(INC_DIR)/*.h)

CLIENT_SRCS = $(SRC_DIR)/client.c	# arquivos exclusivos do cliente
TRACKER_SRCS = $(SRC_DIR)/tracker.c	# arquivos exclusivos do tracker
COMMON_SRCS = $(filter-out $(CLIENT_SRCS) $(TRACKER_SRCS), $(SRCS))

CLIENT_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CLIENT_SRCS))
TRACKER_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(TRACKER_SRCS))
COMMON_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(COMMON_SRCS))

CC=gcc
CC_FLAGS=-c -std=gnu99 -Wall -Wextra -DLOG_USE_COLOR

LD_FLAGS=-lpthread

all: $(BIN_DIR)/tracker $(BIN_DIR)/client

$(BIN_DIR)/tracker: $(TRACKER_OBJS) $(COMMON_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LD_FLAGS)

$(BIN_DIR)/client: $(CLIENT_OBJS) $(COMMON_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LD_FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(OBJ_DIR)
	$(CC) $(CC_FLAGS) $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

debug: CC_FLAGS += -g -O0
debug: clean all

.PHONY: all clean debug
