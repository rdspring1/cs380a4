all:
	g++ -ggdb trace.c -o tracer
	g++ -ggdb -static -Wl,-Ttext-segment=0x200000 loader.c -o loader
	g++ -ggdb -static -Wl,-Ttext-segment=0x200000 demandloader.c -o demandloader
	g++ -ggdb -static -Wl,-Ttext-segment=0x200000 hybridloader.c -o hybridloader
	g++ -ggdb -static pager.c -o pager
	g++ -ggdb -static test1.c -o test1 
	g++ -ggdb -static test2.c -o test2 
	g++ -ggdb -static test3.c -o test3 
	g++ -ggdb -static null.c -o null 

clean:
	rm -f tracer
	rm -f loader
	rm -f demandloader
	rm -f hybridloader
	rm -f pager
	rm -f test1
	rm -f test2
	rm -f test3
	rm -f null
