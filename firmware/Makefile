.PHONY: phony

all: main test data

main: phony
	cd src; make all

test: phony
	cd test; make all

data: main test
	cp src/main_data.c ../tools/matrixJig/main_data.h
	cp test/test_data.c ../tools/matrixJig/test_data.h
