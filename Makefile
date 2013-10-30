all:
	g++ -ggdb -static trace.c -o tracer
	g++ -ggdb -static trace.c -o loader
	g++ -ggdb -static pager.c -o pager
	g++ -ggdb -static test1.c -o test1 
	g++ -ggdb -static test2.c -o test2 

clean:
	rm -f tracer
	rm -f loader
	rm -f pager
	rm -f test1
	rm -f test2
