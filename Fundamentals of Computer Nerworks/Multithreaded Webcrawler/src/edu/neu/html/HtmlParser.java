package edu.neu.html;

import java.io.BufferedReader;
import java.io.IOException;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import org.jsoup.Jsoup;
import org.jsoup.nodes.Document;
import org.jsoup.nodes.Element;
import org.jsoup.select.Elements;

import edu.neu.constants.CrawlerConstants;
import edu.neu.http.Cookies;

public class HtmlParser {
	
	public void searchFlags(String httpResponse){
		
		Document htmlDocument=null;
		if(httpResponse!=null){
			htmlDocument=Jsoup.parse(httpResponse);
			Elements h2=htmlDocument.select("h2");
			for(int i=0;i<h2.size();i++){
				Element h2El=h2.get(i);
				if(h2El.attr("class").contains("secret_flag") || h2El.text().contains("FLAG:")){
					String secretFlagSplit[]=h2El.text().toString().split(": ");
					String secretFlag=secretFlagSplit[1];
					CrawlerConstants.synSearchFlags.add(secretFlag);
					System.out.println("Found flag : *******************************"+h2El.text().toString());
				}
			}
		}
		if(CrawlerConstants.searchFlagsSet.size()==5){
			CrawlerConstants.allFlagsFound=true;
			Iterator<String> iterator=CrawlerConstants.searchFlagsSet.iterator();
			while(iterator.hasNext()){
				System.out.println(iterator.next().toString());
			}
		}
		
	}
	/**
	 * 
	 */
	
	public String parseHtml(BufferedReader bufReader){
		
		String htmlString="";
		String tempString="";
		StringBuffer stringBuffer=new StringBuffer();
		try {
			while((tempString=bufReader.readLine())!=null || bufReader.readLine()!=null){
				stringBuffer.append(tempString);
			}
			htmlString=stringBuffer.toString();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			System.out.println("Error : Could not read from buffer");
		}
		return htmlString;
	}
	/**
	 * 
	 */
	public Cookies setCookieFromHtml(String htmlResponse){
		Cookies cookie=new Cookies();
		String[] splitArray=htmlResponse.split(" ");
		for(int i=0;i<splitArray.length;i++){
			//System.out.println("SplitArray"+splitArray[i]+" "+i);
			if(splitArray[i].contains("csrftoken")){
				String[] splitcsrftoken=splitArray[i].split("=");
				String[] splitSemiColon=splitcsrftoken[1].split(";");
				cookie.setCsrfmiddelWareToken(splitSemiColon[0]);
				System.out.println("csrftoken"+cookie.getCsrfmiddelWareToken());
			    
			}
			if(splitArray[i].contains("sessionid")){
				String[] splitSessionId=splitArray[i].split("=");
				String[] splitSemiColon=splitSessionId[1].split(";");
				cookie.setSesssionId(splitSemiColon[0]);
				System.out.println("sessionId"+cookie.getSesssionId());
			}
		}
		return cookie;
		
	}
	public Integer getHttpStatusCode(BufferedReader buf){
		Integer statusCode=0;
		try {
			String[] s=buf.readLine().split(" ");
			statusCode=Integer.parseInt(s[1]);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		return statusCode;
	}
	/**
	 * 
	 */
	public String getLocation(String httpResponse){
		StringBuffer Location=new StringBuffer();
		StringBuffer tempString=new StringBuffer();
		String splitString[]=httpResponse.split(":");
		int i=0;
		while(!splitString[i].contains("Location")){
			i++;
		}
		tempString=tempString.append(splitString[i+1]+":"+splitString[i+2]);
		int lasIndex=tempString.lastIndexOf("/");
		i=0;
		while(i!=lasIndex+1){
			Location.append(tempString.charAt(i));
			i++;
		}
		//httpResponse.indexOf("Location");
		//System.out.println("Got location : "+Location);
		return Location.toString();
	}

	public synchronized Set<String> addPageAllUrls(String htmlResponse){
		
			
			Set<String> set=new HashSet<String>();
			Document htmlDocument=null;
			if(htmlResponse!=null){
			htmlDocument=Jsoup.parse(htmlResponse);
			Elements href=htmlDocument.select("a[href]");	
			for(int i=0;i<href.size();i++){
				Element link=href.get(i);
				if(link.attr("href").startsWith("/") || link.attr("href").startsWith("http://cs5700.ccs.neu.edu")){
					set.add(link.attr("href"));
					}
				}
			}
			//htmlDocument=
			/*Iterator<String> linksIterator=set.iterator();
			while(linksIterator.hasNext()){
				System.out.println("Links: "+linksIterator.next());
			}*/
		
		return set;
		//System.out.println();
		//return CrawlerConstants.synToDoSet;
	}
	
	/**
	 * 
	 */
	/*public String getNewSessionid(String htmlResponse){
		String sessionId="";
		int i=0;
		String[] splitStrings=htmlResponse.split("");
		while((s=htmlResponse).contains("sessionId")){
			
		}
		return sessionId;
	}*/
}
