# -*- Makefile -*-
CC=gcc
CFLAGS = -Wall -g #-fanalyzer
LIBS=

TARGET = test_main

all : $(TARGET)


$(TARGET) : test_main.o workload.o read_trace.o
	@echo "Building $@"
	@$(CC) test_main.o workload.o read_trace.o -o $@


test_main.o : test_main.c 
	@echo "Building $@"
	@$(CC) $(CFLAGS) -c test_main.c 

workload.o : workload.c workload.h 
	@echo "Building $@"
	@$(CC) $(CFLAGS) -c  workload.c 

read_trace.o : read_trace.c read_trace.h 
	@echo "Building $@"
	@$(CC) $(CFLAGS) -c  read_trace.c 


clean:
	rm -rf $(TARGETS) *.o *.dSYM *~


