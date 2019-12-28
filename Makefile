
analyze_cpp := src/analyze_main.cpp src/analyze.cpp
analyze_obj = $(subst .cpp,.o,$(analyze_cpp))

bin/analyze: Makefile src/analyze.hpp $(analyze_cpp)
	clang++ -std=c++17 -g -O2 -o bin/analyze $(analyze_cpp) -lpthread

bin/analyze_tests: Makefile src/analyze.hpp src/analyze_tests.cpp src/analyze.cpp
	clang++ -std=c++17 -ICatch2/single_include -o bin/analyze_tests src/analyze_tests.cpp src/analyze.cpp

.PHONY:
analyze: bin/analyze
	cd samples && time ../bin/analyze *.dat

.PHONY:
test: bin/analyze_tests
	$< --use-colour no
