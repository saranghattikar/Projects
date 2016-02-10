import httplib
import os
import sys
import socket
import string, cgi, time
from os import curdir, sep
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

class MyHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        fullPath = sep + self.path
        #print(fullPath)
        #print (self.path)
        #fullPath=sep + os.path.basename(self.path)
        filename=os.path.basename(self.path)
        #print ("printing full path")
        #print(fullPath)


        try:
            
                fullPath = sep + self.path

                fHandler = open(fullPath) 
                self.send_response(200)
                self.send_header('Content-type',    'text/html')
                self.end_headers()
                self.wfile.write(fHandler.read())
                fHandler.close()
                return    
                
        except IOError:
            res=requesttoOrigin(self.path)
            if (res.status==200):
               self.send_response(200)
               self.send_header('Content-type',    'text/html')
               #self.send_header(res.getheader())
               self.end_headers()
               self.wfile.write(res.read())
               open(str(filename), 'wb').writelines(res.read())
               #open(str(self.path[int(filename)+1:]), 'wb').writelines(res.read())
               return
            else:
                self.send_error(404,'File Not Found: %s' % self.path)
                


            #print requesttoOrigin(fullPath)
            #self.wfile.write(requesttoOrigin(fullPath))
            #self.send_error(404,'File Not Found: %s' % self.path)



def requesttoOrigin(path):
    try:
        conn = httplib.HTTPConnection("ec2-54-85-79-138.compute-1.amazonaws.com:8080")
        conn.request("GET",path)
        res = conn.getresponse()
        return res
        
    except:
        print("Error")


def main(argv):
    
    PORT = 0
    localip=''
    origin=''
    #print("hello");
    try:
        PORT = int(argv[0])
        if (PORT<=49152 or PORT >=65535):
            print("Invalid PORT")
            sys.exit(0)
            #SystemExit
        origin = str(argv[1])
        if(origin!="ec2-54-85-79-138.compute-1.amazonaws.com"):
            print("Invalid origin server name")
            sys.exit(0)
            #SystemExit

    except Exception, e:
        raise e
        print ("Invalid PORT OR DOMAIN")
        sys.exit(0)
        SystemExit

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
        myHttpServer = HTTPServer((localip, PORT), MyHandler)
        print("Started HTTP Server...")
        myHttpServer.serve_forever()
    except KeyboardInterrupt:
        print("Server Shutting Down...")
        myHttpServer.socket.close()

if __name__ == '__main__':
    main(sys.argv[1:])
