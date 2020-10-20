default: header.h server.cpp client.cpp
	g++ -o server -std=c++11 server.cpp
	g++ -o client -std=c++11 client.cpp
dist: server.cpp README.md Makefile header.h client.cpp
	tar -czf ReliableTransport.tar.gz server.cpp README.md Makefile header.h client.cpp
clean:
	rm -f server client ReliableTransport.tar.gz *.file
test:
	sudo tc qdisc add dev lo root netem loss 10%
endtest:
	sudo tc qdisc del dev lo root
