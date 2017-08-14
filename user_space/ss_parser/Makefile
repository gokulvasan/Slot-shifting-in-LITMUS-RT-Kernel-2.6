SRC=./src
INC=./include/
CONF=./conf/
CC=gcc
CFLAGS= -iquote$(INC) -iquote$(CONF) -DDEBUG 
#-DSYS_HOST
LDFLAGS=
#SOURCES= fsm_parser.c main.c
#OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=ss_parser

#OBJ=$(join $(addsuffix ../obj/, $(dir $(SOURCE))), $(notdir $(SOURCE:.cpp=.o))) 
vpath %.c src/
OBJECTS = $(patsubst src/%.c,%.o,$(wildcard src/*.c))

all:  $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(CFLAGS) $(OBJECTS) -o $@


.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf ./*.o
	rm -rf *o $(EXECUTABLE)
