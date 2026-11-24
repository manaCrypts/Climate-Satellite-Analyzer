CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS = -pthread -lm
SOURCES = src/main.c src/processing.c src/detection.c src/risk.c
TARGET = climate_analyzer

all: $(TARGET)
$(TARGET): $(SOURCES) include/project.h
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)
run: $(TARGET)
	./$(TARGET) --demo --threads 4
clean:
	rm -f $(TARGET)
.PHONY: all run clean
