all:
	g++ -Wall -ggdb -static -Wl,-Ttext-segment=0x200000 loader.c -o loader
	g++ -Wall -ggdb -static -Wl,-Ttext-segment=0x200000 demandloader.c -o demandloader
	g++ -Wall -ggdb -static -Wl,-Ttext-segment=0x200000 hybridloader2.c -o hybridloader2
	g++ -Wall -ggdb -static -Wl,-Ttext-segment=0x200000 hybridloader3.c -o hybridloader3
	g++ -ggdb -static test1.c -o test1 
	g++ -ggdb -static test2.c -o test2 
	g++ -ggdb -static null.c -o null 

clean:
	rm -f loader
	rm -f demandloader
	rm -f hybridloader2
	rm -f hybridloader3
	rm -f test1
	rm -f test2
	rm -f null
