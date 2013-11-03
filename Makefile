all:
	g++ -ggdb trace.c -o tracer
	g++ -ggdb -static -Wl,-Ttext-segment=0x200000 loader.c -o loader
	g++ -ggdb -static pager.c -o pager
	g++ -ggdb -static test1.c -o test1 
	g++ -ggdb -static test2.c -o test2 

clean:
	rm -f tracer
	rm -f loader
	rm -f pager
	rm -f test1
	rm -f test2
