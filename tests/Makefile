# Standalone tests for the filter core. Needs only a C++ compiler -- no Rack,
# no Rack SDK. From this directory:  make       (or: make run)

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -I.
TARGET   := test_dsp

.PHONY: all run clean
all: run

$(TARGET): test_dsp.cpp fmd_test_shim.hpp ../src/FmdDsp.hpp
	$(CXX) $(CXXFLAGS) -o $@ test_dsp.cpp

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
