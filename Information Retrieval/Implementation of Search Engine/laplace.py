import string
import urllib, re
import sys
import os
import math
from bs4 import BeautifulSoup
from htmltextextract import PorterStemmer


# this functions take a term and return updated dict_doc_score which is actually a dictionary 
# containing docid and score of the documents 
 # here oktf_term is okapi value of the term in the query

def extract_queries():
    p = PorterStemmer()
    f1=open('stoplist.txt','r')
    f2=open('edited.txt','w')
    stopwords=f1.read()
    stop_words_list = stopwords.split("\n") 
    with open('queries.txt','r') as f :   
       for line in f.readlines() :
           for punct in string.punctuation:
               line = line.replace(punct, " ") 
           
           words=line.split(" ")
           output=''
           for word in words:
                word=word.lower()
                if word not in stop_words_list :               # taking only words which are not in stopword list
                    output += " " +p.stem(word, 0,len(word)-1)   
           f2.write(output)
    f1.close()
    f2.close()    



def get_index(term,dict_doc_score,doclen_dict,df_dict,qtf,unique_terms,docid_terms):
    
    
    try :
        df=df_dict[term]
    except KeyError :
        df = 0
    
    
    
    
    try:         
        with open('documents/'+term+'.txt','r') as file1 :
            for line in file1.readlines() :
                words=line.split(" ") 
                tf=float(words[2])
                docid=words[1]
                print(doclen_dict[docid])   
               
                p = (float((tf + 1)/(float(doclen_dict[docid]) + unique_terms)))
                
                
                try:
                    dict_doc_score[docid] = dict_doc_score[docid] + math.log(p,10) 
                except KeyError: 
                    dict_doc_score[docid] = math.log(p,10)
            
                if docid not in docid_terms:
                    docid_terms[docid]=[0,[]]
            
                docid_terms[docid][1].append(term)
                    
                    
                    
                
                
               
    
    
    
    except IOError:
        return dict_doc_score 
    
    
    return dict_doc_score     

# this function removes stop words from the given query file.
def stopping_process(): 
    queries_file = 'queries.txt'
    edited_queries = 'processed_queries.txt'
    f = open(queries_file, 'r')
    o = open(edited_queries, 'w')
    f1 = open(edited_queries, 'r')
    stopList = []
    stoplist_file = 'stoplist.txt'
    f1 = open(stoplist_file, 'r')
    queries = []
    
    for line in f1.readlines():
        stopList.append(line.strip('\n'))  # appends stop words to stopList

    for line in f.readlines():
        for punct in string.punctuation:
            line = line.replace(punct, "")  # removes punctuations

    # code for removing stop words from the file and writing to another file
        wordList = line.strip().split(" ")
        for word in wordList :                  
            if ((stopList.count(word)) == 1) : 
                # check whether the word is found in the stop word list and append spaces to it and replace it with space   
                                     
                word = (" " + word + " ")
                line = line.replace(word, " ") 
                line = line.replace("document", " ")  
                
        # writes to a separate file
                                  
        o.write(line[7:])
    f.close()
    
# this functions returns average query length of the query in the file
def avg_query_length():
    f1 = open('edited.txt', 'r') 
    q = [] 
    wordList = []
    length = 0.0
    for line in f1.readlines() :
        words = line.strip("\n").split(" ")
              
        for each in words:
            if each.isalpha():
                wordList.append(each)
        
    length = length + len(wordList)
    
    return (length / 64)    
    
    f1.close()                                
    
#this function get score for every query in the query file 
def get_score(query,doclen_dict,df_dict,qtf,unique_terms,docid_terms) :
    print "in get _score"
    wordList = [] 
    oktf_query_vector = []
    words = query.strip("\n").split(" ")
    dict_doc_score = {}
    #print len(dict_doc_score), "  ", query
    for each in words:
        if each.isalpha():
            wordList.append(each)
    # print wordList        
    query_length = len(wordList)        
    # print length
    
    for term in wordList:
       dict_tf = dict()  # it is a dictionary which contains the term frequency
       oktf = 0.0
          
       dict_tf[term] = wordList.count(term)  # calculates the count of each term in the given query
       # OKTF=tf/(tf + 0.5 + 1.5*querylen/avgquerylen)
       
       
      
       # print oktf       ---- got oktf values 
       # invoke lemur interface on the term
       
       dict_doc_score = get_index(term, dict_doc_score,doclen_dict,df_dict,qtf,unique_terms,docid_terms) 
       
    
    return dict_doc_score   
    
       
# this functions maps internal document ids of documents with external document  
def map_docid():
  docid_map_dict = {}
  f2 = open('DOCUMENT_MAPPING.txt', 'r')
  for line in f2.readlines():
      each_id = line.split()
      docid_map_dict[each_id[0]] = each_id[1]
 
  # print docid_map_dict
  return docid_map_dict
 
def map_doclen():
  doc_len_dict = {}
  f2 = open('DOCUMENT_MAPPING.txt', 'r')
  for line in f2.readlines():
      each_id = line.split()
      doc_len_dict[each_id[0]] = each_id[2]
 
  # print docid_map_dict
  return doc_len_dict 
 
    
def main() :
     # this function performs stopping operation on the given file 
    total_words=169266
    unique_terms=11201.0
    avg_doclen=52.8     
    extract_queries()                                            #                       and returns queries without stop words
    doclen_dict=dict()
    doclen_dict= map_doclen()
    
    edited_queries = 'edited.txt'
    f1 = open(edited_queries, 'r') 
    f2 = open('result_processed_queries_laplace_model.txt', 'w')
    q = []  # stores okapi values in list q
    
    ctf_dict={}
    df_dict={}        
    path1= 'documents/'
    for file4 in os.listdir(path1):
        term_id=0
        ctf=0
        df=0
        if file4.endswith(".txt"):
            f3 = open('documents/'+str(file4),'r')
        for line in f3.readlines() :
            df=df+1
            words = line.split(" ")
            #print(words)
            i=0
            for each in words:
                if i==2:
                    ctf = ctf + int(each) 
                i=i+1
        ctf_dict[os.path.splitext(file4)[0]]= ctf
        df_dict[os.path.splitext(file4)[0]]= df
        
        f3.close()
    
    
    
    
    
    
    map_id = {}  
    map_id = map_docid()  # it is dictionary which contains mappings for internal document ids and external document ids 
    
    dict_ID_score = dict()
    # print map_id
    i = 0                             
    for query in f1.readlines(): 
        docid_terms = dict()
        i = i + 1
        print "computing for query : ", query
        rank = 1
        
        word_list=[] 
        
        listofwords=[]
        qtf=dict()
        print "\n computing for query : ", query
        rank = 1
        word_list = query.strip("\n").split(" ")
        for each in word_list :
            if each != '' :
               listofwords.append(each)
        
                  
         
        doc_score = dict()
         
        query_length = len(words)        
        
        
        
        for term in listofwords:
           qtf[term]= listofwords.count(term) 
        
        
        dict_id_score = {} 
        dict_id_score = get_score(query,doclen_dict,df_dict,qtf,unique_terms,docid_terms)
        # print dict_id_score.keys()
        # for each in dict_id_score :
        
        for eachdoc in docid_terms:
            for eachword in listofwords:
                if eachword not in docid_terms[eachdoc][1]:
                      q=(float(1/(float(doclen_dict[eachdoc]) + unique_terms)))
                      dict_id_score[eachdoc] = dict_id_score[eachdoc] + math.log(q,10)
        
        
        
        
        
        for (docid, score) in sorted(dict_id_score.items(), key=lambda x: x[1], reverse=True):     
              # print map_id[str(docid)]    
             if rank <= 1000:
                 line_of_text = str(i) + "\t" + 'Q0' + "\t" + map_id[str(docid)] + "\t" + str(rank) + "\t" + str(round(score, 6)) + "\t" + 'Exp' + '\n'
                 f2.write(line_of_text)
             else:
      		  break
             rank = rank + 1
             
          
       
    f1.close()
    f2.close()    

if __name__ == "__main__":
    main()
    
