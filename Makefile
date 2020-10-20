default: header.h server.cpp client.cpp
	g++ -o server -std=c++11 server.cpp
	g++ -o client -std=c++11 client.cpp
dist: server.cpp README Makefile header.h client.cpp
	tar -czf 105093055.tar.gz server.cpp README Makefile header.h client.cpp
clean:
	rm -f server client 105093055.tar.gz
test:
	sudo tc qdisc add dev lo root netem loss 10%
endtest:
	sudo tc qdisc del dev lo root
