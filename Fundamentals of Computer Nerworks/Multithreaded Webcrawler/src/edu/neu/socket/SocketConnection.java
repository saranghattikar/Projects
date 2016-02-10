package edu.neu.socket;

import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;

public class SocketConnection {
		/*
		 * Variable declaration block
		 */
		protected 	static 	final 	String 	host					= 		"cs5700.ccs.neu.edu";
		protected	static	final	Integer	port					=		80;
		protected					Socket	httpSocketConnection;  						
		/*
		 * End of variable decalaration
		 */
		
		public static Socket open(){
			
			Socket httpSocketConnection=null;
			try {
				
				httpSocketConnection=new Socket(InetAddress.getByName(host),port);
				
			} catch (UnknownHostException e) {
				// TODO Auto-generated catch block
				System.out.println("Message from crawler: Could not find host\n");
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
				System.out.println("Message from crawler: Error connecting\n");
			}
			catch (Exception e){
				e.printStackTrace();
				System.out.println("Mesage from the crawler: Error connecting\n");
			}
			return httpSocketConnection;
		}
}
