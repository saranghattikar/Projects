
'''
Created on Mar 24, 2014

@author: SarangPC
'''
from struct import *
import socket
import sys


class DNSQUERY:
    def  __init__(self,data,DOMAIN):
        self.data=data
        self.dom=''
	self.b1=True
	#dnsheader = unpack('!h?hhhh?',data[0:12])
	#print("printing msg type")
	#print (dnsheader[2])
        
	#print("printing first two bytes")        
	#print(ord(data[2]))
	tipo=(ord(data[2])>>3)&15
        if tipo == 0:
            ini=12
            lon = ord(data[ini])
            while lon !=0:
                self.dom+=data[ini+1:ini+lon+1]+'.'
                ini+=lon+1
                lon=ord(data[ini])
        #print(self.dom)
	x=self.dom.__len__()
        self.dom=self.dom[0:x-1]
	#print("printing self.dom   "+self.dom)
	#print("printing domain   "+DOMAIN)
	if(self.dom!=DOMAIN):
		#print("Domain not found")
		#return false
		self.b1=False
	
        #return self.dom
	#print (data) 
    
    def response(self,ip):
        print(ip)
        packet=''
        if self.dom:
            packet+=self.data[:2]+"\x81\x80"
            packet+=self.data[4:6]+self.data[4:6]+'\x00\x00\x00\x00'
            packet+=self.data[12:]
            packet+='\xc0\x0c'
            packet+='\x00\x01\x00\x01\x00\x00\x00\x3c\x00\x04'
            packet+=str.join('',map(lambda x: chr(int(x)),ip.split('.')))
        return packet  

def getdestip():
    #bashcommands=[]
    #bashcommands.append("ssh hattikars@ec2-54-84-248-26.compute-1.amazonaws.com "./script.sh" >> out")
    #bashcommands.append(""ssh hattikars@ec2-54-84-248-26.compute-1.amazonaws.com "./script.sh" >> out)
    bashCommand = "bash runscriptonec2.sh"
    os.system(bashCommand)
    bashCommand = "bash parsing_grep.sh"

    ipdict={}
    iparray=[]
    iparray.append('54.84.248.26')
    iparray.append('54.186.185.27')
    iparray.append('54.215.216.108')
    iparray.append('54.72.143.213')
    iparray.append('54.255.143.38')
    iparray.append('54.199.204.174')
    iparray.append('54.206.102.208')
    iparray.append('54.207.73.134')
    
    #print()
    rtt_times=[]
    input_file='out_final.text'
    #d = collections.OrderedDict(sorted(d.items()))
    
    f2 = open(input_file, 'r')
    
    
    for line in f2.readlines():
        rtt_times.append(float(line.strip('\n')))
    
    #print(rtt_times)
    i=0
    for rt in rtt_times:
        ipdict[rt]=iparray[i]
        i+=1
        if i==7:
            break
    
    dest_ip='0'                     #by default give destination ip adress
    print(sorted(ipdict))
    for k in sorted(ipdict):
        dest_ip=ipdict[rt]
        break
    
    
    print(dest_ip)
    return (dest_ip)




            
            
        
        

def main(argv):

    ip="54.85.79.138"  #IP ADDRESS OF ORIGIN SERVER
    PORT = 0
    localip=''
    DOMAIN=''
    #print("hello");
    dest_server_ip=''
    print (getdestip())

    try:
        PORT = int(argv[0])
        if (PORT<=49152 or PORT >=65535):
            print("Invalid PORT")
            SystemExit
        DOMAIN = str(argv[1])
    except Exception, e:
        raise e
        print ("Invalid PORT OR DOMAIN")
        sys.exit(0)
        #SystemExit

    
    try:
        udp_Socket_connection = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        udp_Socket_connection.connect(('www.google.com',80))
        localip = udp_Socket_connection.getsockname()[0]
        print (localip)
        udp_Socket_connection.close()
    except Exception, e:
        raise e
        print("Socket Error")
        sys.exit(0)

    
    try:
        udp_Socket= socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        udp_Socket.bind((localip,PORT))
    except Exception, e:
        raise e
        print("Socket Error")
        sys.exit(0)

    dest_server_ip=''
    try:
        while 1:
            
	    data, addr = udp_Socket.recvfrom(1024)
	    print ("printing socket address")
	    print (addr)
        #dest_server_ip=getdestip()
        query_object= DNSQUERY(data,DOMAIN)
	    if (not(query_object.b1)):

		      #print ("query_object.b1 is false and breaking while loop")
		      #break
            continue		
		
	    else:
            udp_Socket.sendto(query_object.response(ip),addr)
	            #udp_Socket.close()
	            #sys.exit
    except KeyboardInterrupt:
        print("Shutting Down")
        udp_Socket.close()
        sys.exit(0)
        


if __name__ == '__main__':
    main(sys.argv[1:])
