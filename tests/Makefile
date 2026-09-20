# Standalone tests for the filter core. Needs only a C++ compiler -- no Rack,
# no Rack SDK. From this directory:  make       (or: make run)

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -I.
TARGET   := test_dsp
SL_TARGET := test_superlove

.PHONY: all run run-superlove clean
all: run run-superlove

$(TARGET): test_dsp.cpp fmd_test_shim.hpp ../src/common/FmdDsp.hpp
	$(CXX) $(CXXFLAGS) -o $@ test_dsp.cpp

$(SL_TARGET): test_superlove.cpp ../src/modules/SuperLove/SuperLoveFilter.hpp
	$(CXX) $(CXXFLAGS) -I../src/common -I../src/modules/SuperLove -o $@ test_superlove.cpp

run: $(TARGET)
	./$(TARGET)

run-superlove: $(SL_TARGET)
	./$(SL_TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(SL_TARGET) $(SL_TARGET).exe
