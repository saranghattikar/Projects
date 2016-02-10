package edu.neu.http;

public class CrawlerHttpRequest {
				
		public String getGETHeader(String url){
			String getRequest=
					"GET "+url+" HTTP/1.0\n"+
					"Host: cs5700.ccs.neu.edu\n"+
					""+
					"\n";
			//System.out.println(getRequest);
			
			return getRequest;		
		}
		
		/**
		 * 
		 */
		public String getGETHeader(String url, Cookies cookie){
			String getRequest=
					"GET "+url+" HTTP/1.0\n"+
					"Host: cs5700.ccs.neu.edu\n"+
					"Connection: close\r\n"+
					"Accept: */*\r\n"+
					"User-Agent: Java\r\n"+
					"DNT: 1\r\n"+
					"Content-Type: application/x-www-form-urlencoded\r\n"+
					"Cookie: csrftoken="+cookie.getCsrfmiddelWareToken()+";"+" "+"sessionid="+cookie.getSesssionId()+";"+"\r\n\r\n"+
					""+
					"\n";
			//System.out.println(getRequest);
			return getRequest;
		}
		public String getPOSTHeader(String url, Cookies cookie){
			String params = "csrfmiddlewaretoken"+"="+cookie.getCsrfmiddelWareToken()+"&"+"username"+"="+"001990832"+"&"+"password"+"="+"4NEXSQCC"+"&"+"next="+"%2Ffakebook%2F";
			String postRequest=
					"POST "+url+" HTTP/1.0\r\n"+
					"Host: cs5700.ccs.neu.edu\r\n"+
					"Connection: close\r\n"+
					"Accept: */*\r\n"+
					"Content-Type: application/x-www-form-urlencoded\r\n"+
					"Content-Length: " + params.length()+ "\r\n"+
					"Cookie: csrftoken="+cookie.getCsrfmiddelWareToken()+";"+" "+"sessionid="+cookie.getSesssionId()+";"+"\r\n\r\n"+
					params+"";
			//String postRequest=temp;
			//System.out.println("POSTING: "+postRequest);
			return postRequest;
		}
		
		/*
		 * Method: getCookieHeader
		 */
		public String getCookieHeader(Cookies cookie){
			String cookieHeader=
					"csrftoken="+cookie.getCsrfmiddelWareToken()+
					";"+
					" "+
					"sessionid="+
					cookie.getSesssionId()+
					";"+
					"\r\n\r\n";
			return cookieHeader;
		}
		/**
		 * 
		 */
		public boolean isHttpStatusCodeOk(Integer statusCode){
			
			switch(statusCode){
			case 200: 
				System.out.println();
				break;
			case 300: 
				break;
			case 404:
				System.out.println("Message from the crawler : Url not found\n");
				break;
			case 302: 
				System.out.println("Message from the crawler : Permanently moved\n");
				break;
			}
			return true;
		}
}
