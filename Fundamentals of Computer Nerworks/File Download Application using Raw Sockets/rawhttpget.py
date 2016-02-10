######################################################################################
#Import statements
######################################################################################

import socket, sys, random, time
from struct import *

######################################################################################
#End of Import statements
######################################################################################


######################################################################################
#Global variable block
######################################################################################
dataToPush= dict()
expected_ack_number=''
source_ip=''
dest_ip=''
senderSocket=''
packet = ''
recvdPacket=''
receiveSocket=''
source_port=''
getAck_number = ''
url=''
hostname=''
start_time=''
writeFlag=True
######################################################################################
#End of global variable block
######################################################################################


######################################################################################
#Start of helper functions
###################################################################################### 

#Function getSourceIpAddress : to return the Ip address of source 
def getSourceIpAddress():
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	s.connect(('ccs.neu.edu', 80))
	return (s.getsockname()[0],s.getsockname()[1])

#Function getSourcePort : to return source port


#Function getIPHeader : Constructs the IP header
def getIPHeader(source_ip, dest_ip):
	ip_ihl = 5
	ip_ver = 4
	ip_tos = 0
	ip_tot_len = 0  
	ip_id = 56789   
	ip_frag_off = 0
	ip_ttl = 255
	ip_proto = socket.IPPROTO_TCP	
	ip_check = 0    
	ip_saddr = socket.inet_aton ( source_ip )   
	ip_daddr = socket.inet_aton ( dest_ip )
 	ip_ihl_ver = (ip_ver << 4) + ip_ihl
 
	
	ip_header = pack('!BBHHHBBH4s4s' , ip_ihl_ver, ip_tos, ip_tot_len, ip_id, ip_frag_off, ip_ttl, ip_proto, ip_check, ip_saddr, ip_daddr)
	
	return ip_header

#Function getSynTCPHeader : Constructs the tcp header for syn
def getTCPHeader(source_ip, dest_ip, source_port, seq_num, ack_num, tcp_method, user_data):
	tcp_source = source_port   
	tcp_dest = 80   
	tcp_seq = seq_num
	tcp_ack_seq = ack_num
	tcp_doff = 5    
	
	tcp_window = socket.htons (5840)   
	tcp_check  = 0
	tcp_urg_ptr = 0
 
	tcp_flags=getTCPFlags(tcp_method)	
	tcp_offset_res = (tcp_doff << 4) + 0
	
	tcp_header = pack('!HHLLBBHHH' , tcp_source, tcp_dest, tcp_seq, tcp_ack_seq, tcp_offset_res, tcp_flags,  tcp_window, tcp_check, 	tcp_urg_ptr)
	tcp_length=len(tcp_header)+len(user_data)	
	pseudo_header=getPseudoHeader(source_ip, dest_ip,tcp_length)+tcp_header+user_data
	
	tcp_check=calculateCheckSum(pseudo_header)
	tcp_header = pack('!HHLLBBH' , tcp_source, tcp_dest, tcp_seq, tcp_ack_seq, tcp_offset_res, tcp_flags,  tcp_window) + pack('H' , tcp_check) + pack('!H' , tcp_urg_ptr)	
	return tcp_header

		
  
#Function getPseudoHeader : Helper to getPseudoHeader
def getPseudoHeader(source_ip, dest_ip, tcp_length):
	source_address = socket.inet_aton( source_ip )
	dest_address = socket.inet_aton(dest_ip)
	placeholder = 0
	protocol = socket.IPPROTO_TCP
	#tcp_length = len(tcp_header)
 	psh = pack('!4s4sBBH' , source_address , dest_address , placeholder , protocol , tcp_length);
	return psh

#Function getTCPFlags : Constructs the TCP flags
def getTCPFlags(tcp_method):
	tcp_fin = 0
	tcp_syn = 0
	tcp_rst = 0
	tcp_psh = 0
	tcp_ack = 0
	tcp_urg = 0
	if tcp_method=='SYN':
	   tcp_syn=1
	elif tcp_method=='ACK':
	   tcp_ack=1
	elif tcp_method=='SYNACK':
           tcp_syn=1
	   tcp_ack=1
	elif tcp_method=='FIN':
           tcp_fin=1
	elif tcp_method=='RST':
	   tcp_rst=1
	elif tcp_method=='FINACK':
	   tcp_fin=1
           tcp_ack=1
        elif tcp_method=='PSH':
	   tcp_psh=1
	elif tcp_method=='URG':
	   tcp_urg=1
	elif tcp_method=='PSHACK':
	   tcp_psh=1
	   tcp_ack=1	    		 				
	tcp_flags = tcp_fin + (tcp_syn << 1) + (tcp_rst << 2) + (tcp_psh <<3) + (tcp_ack << 4) + (tcp_urg << 5)
	return tcp_flags

#Function getSendSocket : sets the raw socket
def getSendSocket():
	try:
	    rs = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_RAW)
	    #rs.setblocking(0)	
	except socket.error , msg:
	    print 'Socket could not be created. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
	    sys.exit()
	return rs


#Function getReceiveSocket : sets the receive socket
def getReceiveSocket():
	try:
    	    s = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)
        except socket.error , msg:
            print 'Socket2 could not be created. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
            sys.exit()
	return s


#Function calculateCheckSum : Calculates TCP checksum
def calculateCheckSum(message):
	checksum = 0
	#if len(message)%2!=0:
	#   message = message + pack('x')
	if((len(message) % 2) == 0):
		for i in range(0, len(message), 2):
			word_16_bit = ord(message[i]) + (ord(message[i+1]) << 8 )
			checksum = checksum + word_16_bit
	else:
		for i in range(0, (len(message)-1), 2):
			word_16_bit = ord(message[i]) + (ord(message[i+1]) << 8 )
			checksum = checksum + word_16_bit
			
		checksum = checksum + (ord(message[(len(message)-1)]) & socket.ntohs(0xFF00))
	
	checksum = (checksum>>16) + (checksum & 0xffff);
	checksum = checksum + (checksum >> 16);
	
	checksum = ~checksum & 0xffff
	return checksum

#Function unpackTCPHeader : Unpack the tcp header
def unpackTCPHeader(recvdPacket):
	iph_length=getIPHeaderLength(recvdPacket)
	#print ' Unpack ip header length'+str(iph_length)
	#recvdPacket=recvdPacket[0]
	tcp_header=recvdPacket[iph_length:iph_length+20]
	#print 'tcp header length'+str(len(tcp_header))
	tcph = unpack('!HHLLBBHHH' , tcp_header)
	
	#checksum =a tcph[7]
	return tcph


#Function unpackIPHeader : Unpack the IP header
def unpackIPHeader(recvdPacket):
	#recvdPacket=recvdPacket[0]
	ip_header=recvdPacket[0:20]
	ipH=unpack('!BBHHHBBH4s4s' , ip_header)
	return ipH

#Function getServerSeqNumber : get the sequence number received from server
def getServerSeqNumber(recvdPacket):
	
	tcph=unpackTCPHeader(recvdPacket,getIPHeaderLength(recvdPacket))
	


#Function getIPHeaderLength() : get the IP header length of received packet
def getIPHeaderLength(recvdPacket):	
	ipH=unpackIPHeader(recvdPacket)
	ihl_ver = ipH[0]
    	version = ihl_ver >> 4
    	ihl = ihl_ver & 0xF
     	iph_length = ihl * 4
	return iph_length

#Function sendSynTCP() : sends the syn to server 
def sendSynTCP(source_ip, dest_ip, source_port):
	rawSocket	=	getSendSocket()	
	ip_header	=	getIPHeader(source_ip, dest_ip)
	seq_num		=	random.randint(0,4294967295)
	tcp_header	=	getTCPHeader(source_ip, dest_ip, source_port, seq_num, 0 ,'SYN', '')
	packet		=	ip_header+tcp_header
	rawSocket.sendto(packet, (dest_ip , 80))
	rawSocket.close()
	return seq_num

#Function sendAckTCP() : sends the ack to server
def sendAckTCP(source_ip, dest_ip, source_port, seq_num, ack_num):
	rawSocket	= 	getSendSocket()
	ip_header	=	getIPHeader(source_ip, dest_ip)
	#print 'Sending ack to TCP'  	
	tcp_header	=	getTCPHeader(source_ip, dest_ip, source_port, seq_num, ack_num ,'ACK', '')
	packet		=	ip_header+tcp_header
	rawSocket.sendto(packet, (dest_ip , 80))
	#print ' Ack sent'
	rawSocket.close()
	
#Function receivePacket() : receives the packet from server
def receivePacket():
	sock		=	getReceiveSocket()
	while True:	 
	   recvdPacket  = 	sock.recvfrom(65565)
	#print ' Syn ack ' + str(len(recvdPacket[0]))
	   recvdPacket	=	recvdPacket[0]  	
	   tcph		=	unpackTCPHeader(recvdPacket)
	   dest_port	=	tcph[1]
	   #print 'Sequence number ' + str(seq_number) 
	   ack_number	=	tcph[3]
	   #print 'Acknowledge number ' + str(ack_number) 		
	   if dest_port==source_port:
		#print ' Found SYNACK'			
		recvPack=recvdPacket  		
		break	
	sock.close()	
	return recvPack

#Function checkPacketSequence() : checks the packet sequence
#def checkPacketSequence():

	

#Function handleHttpResponse() : unpacks the data from received packet
def handleHttpResponse(seq_num, ack_num):
	sock		=	getReceiveSocket()
	while True:	 
	   recvdPacket  = 	sock.recvfrom(65565)
	#print ' Syn ack ' + str(len(recvdPacket[0]))
	   recvdPacket	=	recvdPacket[0]  	
	   tcph		=	unpackTCPHeader(recvdPacket)
	   dest_port	=	tcph[1]
	  # print 'dest port ' + str(dest_port)
	  # print ' source port ' + str(source_port)
	   seq_number	=	tcph[2]
	  # print 'Sequence number ' + str(seq_number)		 
	   ack_number	=	tcph[3]
	   tcp_flags 	=	tcph[5]
	   data		=	extractData(recvdPacket, tcph,getIPHeaderLength(recvdPacket))	
	   
	   	
		
	   if dest_port==source_port and len(data)!=0 :
				
		line = data.split("\n")
		if line[0].startswith("HTTP"):
		    if line[0].find("200")==-1:
			print 'Error : could not find resource'
			writeFlag=False	
		    
			
						
		if tcp_flags & 0x01==1:			#check fin tcp flag 
			dataToPush[seq_number]=data 
			sendFinAckTCP(source_ip, dest_ip, source_port, ack_number, seq_number+len(data))		
		else:	
							
			dataToPush[seq_number]=data
			sendAckTCP(source_ip, dest_ip, source_port, ack_number, seq_number+len(data))
  	   	
		
	   elif tcp_flags==0x019:
		#print ' Got Fin ack push ' 
		sendFinAckTCP(source_ip, dest_ip, source_port, ack_number, seq_number+1)		
		break
	   elif tcp_flags==0x010 and getAck_number == ack_number:
		if (time.time()-start_time)>1:
		   sendHttpRequest(url, hostname, seq_num, ack_num)		
	sock.close()	
	
	
#Function to vaildate check sum for received packets
def isCorrectCheckSum(tcp_header, user_data):
        tcph0,checksum = reconstructTCPHeader(tcp_header)
	
	tcp_length   = len(tcph0)+len(user_data)	
	pseudo_header= getPseudoHeader(dest_ip, source_ip,tcp_length)+tcph0+user_data
	calculatedChecksum = calculateCheckSum(pseudo_header)
	#	
	if calculatedChecksum == checksum:
	   return True
	else:	
	   return False



#Function reconstructTCPHeader() : Reconstruct tcp header
def reconstructTCPHeader(tcp_header):
	
	
	tcph0 = pack('!HHLLBBHHH',tcp_header[0],tcp_header[1],tcp_header[2],tcp_header[3], tcp_header[4], tcp_header[5], tcp_header[6],0,tcp_header[8]) 
		
	checksum = tcp_header[7] 
		
	return tcph0, checksum


#Function sendFinAcktTCP : to send ack to the Fin flag received 
def sendFinAckTCP(source_ip, dest_ip, source_port, seq_num, ack_num): 
	rawSocket	= 	getSendSocket()
	ip_header	=	getIPHeader(source_ip, dest_ip)
	tcp_header	=	getTCPHeader(source_ip, dest_ip, source_port, seq_num, ack_num ,'FINACK', '')
	packet		=	ip_header+tcp_header
	rawSocket.sendto(packet, (dest_ip , 80))
	rawSocket.close()


#Function extractData() : extracts the data from the http response 
def extractData(packet, tcph, iph_length):
	doff_reserved = tcph[4]
   	tcph_length   = doff_reserved >> 4
	header_size   = iph_length + tcph_length * 4
    	data_size     = len(packet) - header_size
	data 	      = packet[header_size:]
	return data  
	   
	
#Function getHttpGetHeader() : write the http get header 
def getHttpGETHeader(url, hostname):
       getRequest="GET "+url+" HTTP/1.0\r\n"
       getRequest=getRequest+"Host: "+hostname+"\r\n"
       getRequest=getRequest+"Connection: close\r\n\r\n"		  	
       return getRequest

#Function sendHttpRequest() : send Http request
def sendHttpRequest(url, hostname, seq_num, ack_num):
	rawSocket	= 	getSendSocket()
	httpHeader	=	getHttpGETHeader(url, hostname)
	ip_header	=	getIPHeader(source_ip, dest_ip)
	getAck_number   =       seq_num+len(httpHeader)	
	tcp_header	=	getTCPHeader(source_ip, dest_ip, source_port, seq_num, ack_num ,'PSHACK', httpHeader)
	packet		=	ip_header+tcp_header+httpHeader
	rawSocket.sendto(packet, (dest_ip , 80))
	start_time = time.time()
	rawSocket.close()
	handleHttpResponse(seq_num, ack_num) 
		
#Function writeDataToFile() : writes the received data to file
def writeDataToFile(filename):
	s=''	
	string=''	
	for k, v in sorted(dataToPush.items()):
	    s=str(v)
	    if s.startswith('HTTP'):		    
		string=string+s.split("\r\n\r\n", 1)[1]
	    else:
		string=string+s	
	try:	
	    file = open(filename, "wb")
	    file.write(string)
	    file.close()	
	except error:
	    print ' Could not write to file'
	    sys.exit()				 
		
#Function getFileNameFromUrl() : Gets the filename from url
def getFileNameFromUrl(url):
	url=str(url)
	startIndex =  url.rfind("/")
	filename   =  url[startIndex+1:]
	if filename == "":
	   filename = "index.html" 
	#filename = splitUrl[len(splitUrl)-1]
	
	return filename

#Function getHostName() : Gets the GET url from the url string
def getHostName(url):
	hostname = str(url)
	hostname = hostname[hostname.find("//")+2:]
	endIndex = hostname.find("/")
	if endIndex == -1:
	  hostname = hostname[:len(hostname)]
	else:
	  hostname = hostname[:hostname.find("/")]
	return hostname 	

#Function getUrl() : Gets the GET url from the url string		
def getUrl(url):
	httpUrl = str(url)
	httpUrl = httpUrl[httpUrl.find("//")+2:]
	startIndex = httpUrl.find("/")
	if startIndex == -1:
	   httpUrl = "/"
	else:
	   httpUrl = httpUrl[httpUrl.find("/"):]
	return httpUrl	
###############################################################################################################
#End of helper functions
###############################################################################################################


###############################################################################################################
#Tasks
###############################################################################################################


	
	
	
###############################################################################################################
#Begin main block
###############################################################################################################

url 			=	sys.argv[1]
if url.find("http://")==-1:
   url = "http://"+url
dest_ip 		= 	socket.gethostbyname(getHostName(url))
source_ip, source_port	=	getSourceIpAddress()
#senderSocket		=  	getSendSocket()
#receiveSocket		=	getReceiveSocket()

#Perform TCP handshake

seq_num			=	sendSynTCP(source_ip, dest_ip, source_port)
#print ' Syn sent'
expected_ack_number	=	seq_num+1
recvdPacket		=	receivePacket()
tcph			=	unpackTCPHeader(recvdPacket)
seq_number		=	tcph[2]
ack_number		=	tcph[3]
#print 'After receiving packet seq' + str(seq_number)
#print 'After receiving packet ack' + str(ack_number) 

sendAckTCP(source_ip, dest_ip, source_port, ack_number, seq_number+1)	
#print 'Ack sent ' 	  

#Perform Http request
HttpUrl			=	"/classes/cs4700sp14/50MB.log"	# Url command line
#print 'Complete url'
#print sys.argv[1]


hostname 		=	getHostName(url)	#hostname
HttpUrl			=	getUrl(url) 
filename		=	getFileNameFromUrl(HttpUrl)
sendHttpRequest(HttpUrl, hostname, ack_number, seq_number+1)
#recvdPacket 		=	receivePacket()

#Write to file
if writeFlag:
   #print 'write flag is true'	
   writeDataToFile(filename)


#################################################################################
###End of file
#################################################################################





###############################################################################################################
#End of main block
###############################################################################################################



