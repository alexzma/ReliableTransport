# Design
The server and client initialize their sequence numbers to random values, and 
base their ack values on the value of the incoming headers. The server and 
client use perpetual while loops to run their programs. At the beginning of 
each loop, they check if there are any messages that have timed out, and 
resend those that have. Then, they read in a message. If there are no messages 
to read, they go back to the beginning of the loop. If there is a message, the 
message is read into a buffer. If that message contains an acknowledgement 
number, then the message to be acknowledged is marked as acknowledged. If the 
handshake has not occurred, then only handshake messages are considered. If the 
handshake has occured, then the client sends file parts if it has any, and fin 
messages if it does not. The server sends ack messages in response to the file 
parts, and an ack message and a fin message in response to a fin message. The 
fin messages are in the shutdown sequence, and the connection closes after 
the client sends an ack in response to the server's fin message. The protocol 
used is selective repeat.

# Problems
I had trouble converting from integers and longs to bits and back again. I 
eventually figured out how to convert the values by trial and error with bit 
operators in a standalone main method. Then, I placed all my conversions in 
the header.h file, so I would not make mistakes with having the conversions 
in multiple places, and any mistakes could be fixed easily.

I had difficulty making a statically allocated window of messages to send. 
Since number of sequence numbers is exactly divisible by the packet size, the 
packets eventually repeat sequence numbers. Therefore, the client would have 
to track when a packet became out of range in the array. I changed the window 
to a dynamically allocated vector, which made keeping track of the window much 
easier.

I had difficulty making the ack number match the packet it acknowledges. I had 
off by one errors in the handshake, which caused infinite loops. I fixed this 
by basing the ack numbers directly based on the sequence number of the packet 
being sent, rather than having it based on the value of the sequence number 
of the next packet. This ensured that the ack number received would always be 
the same as the ack number expected.

# Libraries
errno.h
string.h
string
vector
chrono
tuple
sys/types.h
sys/socket.h
sys/stat.h
fcntl.h
netdb.h
arpa/inet.h
unistd.h
stdio.h
stdlib.h

# Online Resources
I used the below online resources to access the linux man pages.
Open Group Base Specifications Issue 6 (pubs.opengroup.org)
die.net (linux.die.net)
man7.org