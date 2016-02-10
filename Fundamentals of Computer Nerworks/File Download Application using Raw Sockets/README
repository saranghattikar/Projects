README

FCN - Fundamentals of Computer Networks
Project 4: Raw Sockets Programming

Team Name - mjnsh

Team Members:	
1) Mayur Joshi
NEU ID - 001998032
e-mail ID - mayuruj@ccs.neu.edu

2) Sarang Hattikar
NEU ID - 001998049
e-mail ID - sarangh@ccs.neu.edu

	
Project Approach:

1) We started conceptualizing our project by revisiting the concepts of Three-way handshake and TCP/IP protocol.
 After getting re-familiarized with the concept, we outlined the flow of control of our code.

2) We developed the code using python.We used Silver Moon's tutorial as a referance.
 First step in the implementation was to construct TCP and IP headers.   

3) After successfully constructing the IP and TCP headers, we did trial runs to analyze server's response. We used "Wireshark"
for analysis purpose.

4) Once we completed the three-way handshake, we now had to send request to fetch the code. We then constructed
 the code to send the GET request and again checked the response using "Wireshark".

5) We then developed the code to handle the response, strip off the headers, and append the data contained
 in packets and store the final complete data into a file in the same directory.

6) After receiving FIN from server, we checked for data within incoming FIN packet. If data was there, we extracted it and
 appended it into our aggregated data and sent an ACK to the server and closed the connection.

Challenges:

1) Main challenge was TCP Checksum calculation for odd octets i.e. bytes, so after carefully reading through we
   found, last extra byte needs to be converted into 16-bit word by padding 0s to its right,
   It worked as voila..we were so happy.
2) Again main issue was TCP connection tear-down, we implemented it perfectly, as wireshark was not helpful in 
   this particular scenario, we had to study handshakes through books.
3) Sequencing using python dictionary required us to study python languages built in data structures.

Conclusion:
We learned how the underlying process which takes place to get a single page over the internet and how to construct
the TCP and IP headers within our code. It was seriously a great learning atleast for us.
