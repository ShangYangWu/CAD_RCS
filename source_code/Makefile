CC=g++
FLAGS=-O3 -Wall -Wextra
SOURCES=M11215084.cpp parser.cpp heuristic.cpp algorithm.cpp validate.cpp ilp.cpp ilp_parser.cpp
OBJECTS=$(SOURCES:.cpp=.o)
HEADERS=arg_parser.hpp parser.hpp heuristic.hpp algorithm.hpp validate.hpp ilp.hpp ilp_parser.hpp
EXECUTABLE=M11215084


all: $(SOURCES) $(EXECUTABLE)


$(EXECUTABLE): $(OBJECTS)
	$(CC) $(FLAGS) $(OBJECTS) -o $@

.cpp.o: $(HEADERS)
	$(CC) $(FLAGS) $< -c -o $@

clean:
	rm -rf $(EXECUTABLE) $(OBJECTS)
