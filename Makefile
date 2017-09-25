cmake-release:
	mkdir -p build/release
	cd build/release && cmake -DCMAKE_BUILD_TYPE=Release -DBOOST_ROOT=${LINUXBREWHOME} ../..

build-release:
	cd build/release && make -j4

test-release: build-release
	cd build/release && make test

cmake-debug:
	mkdir -p build/debug
	cd build/debug && cmake -DCMAKE_BUILD_TYPE=Debug -DBOOST_ROOT=${LINUXBREWHOME} ../..

build-debug:
	cd build/debug && make -j4

test-debug: build-debug
	cd build/debug && make test

clean:
	rm -rf build

.PHONY: cmake-release build-release test-release cmake-debug build-debug test-debug clean

