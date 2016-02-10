package edu.neu.bot;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.Socket;
import java.sql.ClientInfoStatus;
import java.util.HashSet;
import java.util.Iterator;
import java.util.ListIterator;
import java.util.Set;


import edu.neu.socket.SocketConnection;
import edu.neu.constants.CrawlerConstants;
import edu.neu.html.HtmlParser;
import edu.neu.http.Cookies;
import edu.neu.http.CrawlerHttpRequest;
public class CrawlerBot {

	/**
	 * @param args
	 */
	private						Socket					socketConnection;
	private						OutputStream			crawlerOutputStream;
	private						InputStream				crawlerInputStream;
	private						InputStreamReader 		crawlerInputStreamReader;
	private						BufferedReader 			crawlerBufferedReader;
	private						HtmlParser				htmlParser;
	private 					CrawlerHttpRequest		crawlerHttpRequest;
	private 	static  final	String					TAG							=		"CrawlerBot";
	private		static			Cookies					cookie;
	private 	static			Cookies					updatedCookies;
	private		static			Iterator<String> 		iter;
	/**
	 * 
	 * 
	 */
	public void init(){

		try {
			openConnection();
			crawlerOutputStream.write(crawlerHttpRequest.getGETHeader("/accounts/login/")
					.getBytes());
			crawlerOutputStream.flush();
			HtmlParser htmlParser=new HtmlParser();
			String httpResponse=htmlParser.parseHtml(crawlerBufferedReader);
			cookie=htmlParser.setCookieFromHtml(httpResponse);
			closeConnection();
			openConnection();
			crawlerOutputStream.write(crawlerHttpRequest.getPOSTHeader("/accounts/login/", cookie)
					.getBytes());		
			crawlerOutputStream.flush();
			if(htmlParser.getHttpStatusCode(crawlerBufferedReader)==302){
				System.out.println("Message from "+TAG+": Login successfull!!");
				httpResponse=htmlParser.parseHtml(crawlerBufferedReader);
				closeConnection();
				openConnection();
				updatedCookies=new Cookies();
				updatedCookies.setCsrfmiddelWareToken(cookie.getCsrfmiddelWareToken());
				updatedCookies.setSesssionId(htmlParser.setCookieFromHtml(httpResponse).getSesssionId());
				System.out.println("Before get");
				crawlerOutputStream.write(crawlerHttpRequest.
						getGETHeader("/fakebook/",
								updatedCookies).getBytes());
				crawlerOutputStream.flush();
				System.out.println("Ater get");
				CrawlerConstants.synToDoSet.add("/fakebook/");

				System.out.println("Before getting response");
				httpResponse=htmlParser.parseHtml(crawlerBufferedReader);
				//httpResponse
				System.out.println("Http Response : "+httpResponse);
				CrawlerConstants.synToDoSet.addAll(htmlParser.addPageAllUrls(httpResponse));
				System.out.println("After add all");
				closeConnection();
				startCrawl();
			}



			closeConnection();



		} catch (IOException e) {
			// TODO Auto-generated catch block
			System.out.println("Error : Outputstream ");
		}
		catch(Exception e){
			e.printStackTrace();
			System.out.println("Message from the crawler: Error connecting");
		}
	}
	public void startCrawl(){
		System.out.println("In start crawl");
		if(!CrawlerConstants.synToDoSet.isEmpty()){
			System.out.println("synToDoSet is not empty");
			while(true){
				System.out.println("Inside while");
				Set<String> hashsetUrl=doTraversal(CrawlerConstants.synToDoSet);
				
				if(hashsetUrl!=null){
				
					CrawlerConstants.synToDoSet.addAll(hashsetUrl);
				}	
			}
		}

	}

	public  synchronized Set<String> doTraversal(Set<String> hashSetUrl){
		System.out.println("Inside do traversal....hashset size"+hashSetUrl.size());
		Set<String> set=new HashSet();
		//System.out.println("crawling :"+url+"++++++++++++++++++++++++++\n");
		Iterator<String> iter=hashSetUrl.iterator();
		try {
			while(iter.hasNext()){
				
				
				String s=iter.next();
				
				if(!CrawlerConstants.synUrlSet.contains(s)){
					System.out.println("crawling :"+s+"++++++++++++++++++++++++++\n");
					System.out.println("%%%%%%%%%%%%%Visited url %%%%%%%% "+s);
					
						openConnection();
						HtmlParser htmlParser = new HtmlParser();
				
						crawlerOutputStream.write(crawlerHttpRequest.getGETHeader(s,updatedCookies)
							.getBytes());
						crawlerOutputStream.flush();
						//crawlerHttpRequest.handleHttpStatusCode(htmlParser.getHttpStatusCode(crawlerBufferedReader));
						//handle all http 
						String htmlResponse=htmlParser.parseHtml(crawlerBufferedReader);
						htmlParser.searchFlags(htmlResponse);
						set.addAll(htmlParser.addPageAllUrls(htmlResponse));
						//CrawlerConstants.synToDoSet.remove(s);
						CrawlerConstants.synUrlSet.add(s);
						


					//CrawlerConstants.toDoSet.remove(url);
					/*Iterator<String> iterator=set.iterator();
							while(iterator.hasNext()){
								System.out.println(iterator.next());
							}*/
					// TODO Auto-generated method stub

					//CrawlerConstants.synToDoSet.addAll(doTraversal(iter.next()));
					if(CrawlerConstants.allFlagsFound){
						System.exit(1);
					}




					//startCrawl();
				
				//return set;

				//crawlerOutputStream.flush();

				}
			}
				
			
		} catch (IOException e) {
			// TODO Auto-generated catch block
			System.out.println("Error: Could not process traversal");
		}
		catch(Exception e){
			e.printStackTrace();
			System.out.println("Error: Could not process traversal");
		}
		return set;
	}


			/**
			 * 
			 */
			public void openConnection(){
				try {
					socketConnection=SocketConnection.open();
					if(socketConnection==null){
						System.out.println("Socket connection is null");
					}

					crawlerHttpRequest=new CrawlerHttpRequest();
					htmlParser=new HtmlParser();

					crawlerOutputStream=socketConnection.getOutputStream();
					crawlerInputStream=socketConnection.getInputStream();
					crawlerInputStreamReader=new InputStreamReader(crawlerInputStream);
					crawlerBufferedReader=new BufferedReader(crawlerInputStreamReader);
				} catch (IOException e) {
					// TODO Auto-generated catch block
					System.out.println("Error : Could not initialize connection\n");
				}catch(Exception e){
					System.out.println("Error : Could not connect\n");
				}

			}

			/**
			 * 
			 */
			public void closeConnection(){
				try {

					crawlerBufferedReader.close();
					crawlerInputStreamReader.close();
					crawlerInputStream.close();
					crawlerOutputStream.close();
					socketConnection.close();
				} catch (IOException e) {
					// TODO Auto-generated catch block
					System.out.println("Error : Could not terminate connection\n");
				}

			}
			public synchronized void addUrlToDoSet(String url){
				try{

				}
				catch(Exception e){

				}
			}

			public synchronized void traverseUrl(){
				try{

				}
				catch(Exception e){

				}
			}
		}
