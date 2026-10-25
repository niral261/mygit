CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -D_FILE_OFFSET_BITS=64
LDLIBS = -lz

.PHONY: all clean test sanitize
all: mygit

mygit: $(wildcard src/*.cpp include/*.h)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/*.cpp -o $@ $(LDLIBS)

test: mygit
	python3 tests/integration.py ./mygit

sanitize:
	$(CXX) $(CPPFLAGS) -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie src/*.cpp -o mygit-sanitize $(LDLIBS)
	ASAN_OPTIONS=detect_leaks=0 python3 tests/integration.py ./mygit-sanitize

clean:
	rm -f mygit mygit-sanitize
