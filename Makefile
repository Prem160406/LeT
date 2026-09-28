# Makefile for Lab Operations, Emergency Fault Triage & Calibration Engine
# VIT Pune, Semester 3, Group 8, IT Department
# Toolchain: MinGW GCC (C++14), Windows Socket 2 (ws2_32)

CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lws2_32

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
TARGET = lab_engine.exe

SRCS = $(SRC_DIR)/Asset.cpp \
       $(SRC_DIR)/CircularQueue.cpp \
       $(SRC_DIR)/HashTable.cpp \
       $(SRC_DIR)/DoublyLinkedList.cpp \
       $(SRC_DIR)/MaxHeap.cpp \
       $(SRC_DIR)/AVLTree.cpp \
       $(SRC_DIR)/UndoStack.cpp \
       $(SRC_DIR)/MergeSort.cpp \
       $(SRC_DIR)/Graph.cpp \
       $(SRC_DIR)/Engine.cpp \
       $(SRC_DIR)/HttpServer.cpp \
       $(SRC_DIR)/main.cpp

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

test: $(TARGET)
	./$(TARGET) --test

server: $(TARGET)
	./$(TARGET) --server 8080

clean:
	-rmdir /s /q $(OBJ_DIR) 2>nul
	-del /f /q $(TARGET) 2>nul

.PHONY: all test server clean
