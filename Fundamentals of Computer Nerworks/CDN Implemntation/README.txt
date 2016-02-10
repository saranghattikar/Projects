Fundamentals of Computer Networks
Project 5: IMplementation of CDN

Introduction:
1.Developed a DNS redirection
2.Developed a http cacheshing server

Approach:
For developing a DNS server is implemented in following manner:
1. Create an instance of socket, bind the socket to a port given as input.
2. Listen to socket for incoming DNS query.
3. Unpack DNS packet to find out the question.
4. Compare question with domain given as input.
5. if input domain is same then return as origin ip addres answers packet
6. else ignore the input query packet

Performance Enhancing:
DNS SERVER:

Design Decisions:

High level approach:
1. Average RTT between DNS server and HTTP server is used map client to  any of available http server.
2. Average RTT is used because network measurements are highly dynamic and subject to change otr vary at every fraction of second

Details of implementations:

1. We decided to use Scamper tool for performance analysis purpose.
2. After a specific interval of time a script initially placed on every ec2 server is executed though DNS server.
3. This is implemented using multithreading so that DNS server should be able to serve incoming DNS queries within this time
4. Thus a thread is created using class RunScamper. This class contains methods to create and handle newly created thread. This thread executes script ‘runscriptonec2’ which in turn executes  scamper on every http server and returns output to DNS server .  
5. ‘runscriptonec2’ placed at DNS server is used to call ‘Script’ placed at every http server.  
6. ‘Script’ executes scamper on every http server. 
6. Output of all this executions is obtained at DNS server. These outputs mainly consist of average minimum and maximum RTT for DNS server and corresponding httpserver.
7. Only average RTT is considered for analysis purpose.
8. To extract average RTT vales from all Scamper outputs awk and grep is used.
9. A script ‘parsing_grep’ is used which contains awk and grep command to parse outputs obtained from running scamper on all servers.
10. A code is written in python to return least RTT and ip address of corresponding http server.
11. Finally a DNS redponse packet having ip address of http replica server is returned to the server.
=================================================================
For http caching Server following steps are implemented:
1. Accept httprequest from client 
2. check whether requested object is available on the server
3. if present then return that resource to client
4. else send a http request to origin server to fetch that object
5. save it on current server and send it to client as well in the response

Performance Enhancing:
HTTP SERVER:

Design Decisions & Implementation details:
1. Caching is implmented in http server so that every http server will save copies of objects at locally.
2. Caching is implemented using data structure queue. Here queue acts as priority queue. 
3. Replacement policy used in caching is purpose is LRU (Least Recently Used)
4. for implementation of LRU file entry is maintained in the queue for cached files. 
5. before caching any file on to the server vailable free space is checked on the server.
6. if sufficent space is not available then a file is removed from the queue and simultaniously deleted from the cache as well.
7. in this way queue is used to impleent lru replacement policy.


Challenges Faced:
1. Packing and unpacking DNS packet
2. Implementing caching  
3. testing code
4. passwordless ssh implemenation
5. procedure for deploying the coomplete project

Extra Implementation if time is given:
1. We will try to implement geolocation and other network measuring tools to improve efficiency.



