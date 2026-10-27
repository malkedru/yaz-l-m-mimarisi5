# LogFlow v5 - derleme ve test
#   make            -> logflow programini derler
#   make test       -> tum testleri derler ve calistirir
#   make experiment -> filtre sirasi deneyini calistirir
CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -pedantic -O2

HEADERS := $(wildcard *.h) $(wildcard tests/*.h)
TESTS   := ParserStageTests PredicateFilterStageTests ProjectionStageTests \
           FileSinkTests PipelineBuilderTests CommandLineTests PipelineLoaderTests ErrorHandlingTests IntegrationTest

all: logflow

logflow: main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp

build/%: tests/%.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

build/OrderExperiment: experiments/OrderExperiment.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

test: $(addprefix build/,$(TESTS))
	@for t in $(TESTS); do echo "== $$t"; ./build/$$t > /dev/null || { ./build/$$t; exit 1; }; done; \
	 echo "Tum test dosyalari gecti."

experiment: build/OrderExperiment
	./build/OrderExperiment

clean:
	rm -rf build logflow *.tmp

.PHONY: all test experiment clean
