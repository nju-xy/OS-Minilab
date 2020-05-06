.PHONY: test libkvdb

test: libkvdb-64# libkvdb-32
	@echo "==== TEST 64 bit ===="
	@LD_LIBRARY_PATH=.. ./libkvdb-64
#	@echo "==== TEST 32 bit ===="
#	@LD_LIBRARY_PATH=.. ./libkvdb-32

libkvdb:
	@cd .. && make -s

libkvdb-64: main.c libkvdb
	gcc -pthread -I.. -L.. -m64 main.c -o libkvdb-64 -lkvdb-64

#libkvdb-32: main.c libkvdb
#	gcc -pthread -I.. -L.. -m32 main.c -o libkvdb-32 -lkvdb-32

clean:
	rm -f libkvdb-32 libkvdb-64 *.db *.log
