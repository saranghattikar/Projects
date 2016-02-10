package edu.neu.constants;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class CrawlerConstants {
		
		public 		static 			Set<String> 		urlSet			=	new HashSet<String>();
		public      static			Set<String>			synUrlSet		=   Collections.synchronizedSet(urlSet);
		public 		static			Set<String> 		searchFlagsSet	= 	new HashSet<String>();
		public 		static			Set<String>			synSearchFlags	= 	Collections.synchronizedSet(searchFlagsSet);
		public 		static			Set<String>			toDoSet	  		= 	new LinkedHashSet<String>();
		public 		static			Set<String>			synToDoSet		= 	Collections.synchronizedSet(toDoSet);
		public 	    static			boolean				allFlagsFound	=	false;
}
