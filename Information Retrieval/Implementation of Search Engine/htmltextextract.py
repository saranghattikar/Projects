import string
import  urllib2
import re
from bs4 import BeautifulSoup
import sys
import os
def main():
             
    path = 'CACM Collection/'
    stopList = []
    stoplist_file = 'stoplist.txt'
    f2 = open(stoplist_file, 'r')
    uniqueWordDict={}
    uniqueWordCount=0
    
    for line in f2.readlines():
        stopList.append(line.strip('\n'))
     
    
    for file in os.listdir(path):
        if file.endswith(".html"):
            f = open('CACM Collection/'+str(file),'r')
            int_doc_id=re.sub('\D',"",file)
    #f = open('CACM-0001.html','r')
            text=f.read()
            soup=BeautifulSoup(text)
            data = soup.get_text()
            x=re.sub("[0-9]+\t[0-9]+\t[0-9]+\n","",data)
            
            y = str(x)
            text1 = y.replace("\n", " ")
            for punct in string.punctuation:
                    text1 = text1.replace(punct, " ")
            words = text1.split(" ")
            #print(words)
            p = PorterStemmer()
            output = ''
            wordlist= []
            wordDict={}
            for word in words:
                word1=word.lower()
                if (word1.isalnum()and word1 not in stopList):
                     
                    #not(stopList.__contains__(word1))
                    output = p.stem(word1, 0,len(word1)-1)
                    x=output.lower()
                    wordlist.append(x)
            #print wordlist
            
            for word in wordlist:
                wordDict[word]= wordlist.count(word)
                                #print(wordDict)
                
     
            doc_length=wordlist.__len__()
     
            i=0
            for term in wordDict.keys():
                if term == 'con' :  
                    term = 'con_term'
                if not(uniqueWordDict.has_key(term)):
                    uniqueWordCount= uniqueWordCount+1
                    uniqueWordDict[term]=uniqueWordCount
                   
                     
                i=i+1
                #===============================================================
                # if term == 'con' :  
                #     term = 'con_term'
                #===============================================================
                termname="documents/" + term + ".txt"
                
                term_id = str(term)+"_"+str(uniqueWordDict[term])
                #term_id = str(uniqueWordDict[term])
                with open(termname,'a+') as file1 :
                    file1.write(str(term_id)+" "+str(int_doc_id)+" "+str(wordlist.count(term)))
                    (file1.write("\n"))
            
            f5 = open('DOCUMENT_MAPPING.txt','a+')
            docname= re.sub(".[h][t][m][l]","",file)
            f5.write(str(int_doc_id)+" "+ str(docname)+" "+str(doc_length))
            f5.write("\n")
    f5.close()
            
    
    
    
    
    
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
                    
    print(ctf_dict.__len__())
    total_terms=0
    for key in ctf_dict.keys():
        total_terms = total_terms + ctf_dict[key]
    print(str(total_terms))
    
     
    
    
    
    
    
    
    
    
    path1= 'documents/'
    textex=""
    offset=0
    for file2 in os.listdir(path1):
        #term_id = term_id+1
        
        if file2.endswith(".txt"):
            f3 = open('documents/'+str(file2),'r')
            for line in f3.readlines() :
                words = line.split(" ")
            term_id= words[0]
            term = os.path.splitext(file2)[0] 
            #print(term_id)
            #print(term)
            f3.seek(0)
            with open('INVERTED_INDEX.txt','a+') as file9 :
                file9.write(f3.read())
                file9.write("\n")
            
            
            f3.seek(0)
            textex = f3.read()
            length= textex.__sizeof__()
            
            with open('TERM_MAPPING.txt','a+') as file10 :
                file10.write(str(term)+" "+str(term_id)+" "+str(offset)+" "+str(length)+" "+ 
                             str(ctf_dict[term])+" " +str(df_dict[term]))
                file10.write("\n")
            #offset= offset + f3.__sizeof__()
            #print(str(f3.__sizeof__()))
            #print(str(len(f3)))
            offset=offset+ length
            #print(str(textex.__sizeof__()))
            
            #print(str(f3.read()))
           
            #===================================================================
            # with open('INVERTED_INDEX.txt','a+') as file9 :
            #     file9.write(f3.read())
            #     file9.write("\n")
            #===================================================================
            
            #print(file2)
            
            #===================================================================
            # print(words)
            # term_id=words[0]
            # term=re.sub('\S',"",term_id)
            # print(term_id)
            # print(term)    
            #===================================================================
            #with open('INVERTED_INDEX.txt','a+') as file9 :
            
            
                
            #term_text = os.path.splitext(file2)[0]
            #print(term_text)
            #com_term_id=str(term_text)+"_"+str(term_id)
            #for line in f3.readlines() :
             #   with open('INVERTED_INDEX.txt','a+') as file9 :
                    #file9.write(str(com_term_id)+" "+str(line))
                    #(file9.write("\n"))
        
            
            
            
            
    
        #f4 = open('documents/'+str(file4),'a+')
        #f4.seek(2)
        #f4.write("ctf"+" "+str(ctf)+"  "+"df"+" "+str(df))
        #f4.close()
    #print (ctf_dict)
    #print(str(ctf_dict.__len__()))
    
    #===========================================================================
    # path1= 'documents/'
    # term_id1=0
    # for file5 in os.listdir(path1):
    #     term_id1 = term_id1+1
    #     if file5.endswith(".txt"):
    #         f3 = open('documents/'+str(file2),'r')
    #         term_text = os.path.splitext(file5)[0]
    #===========================================================================
        
     
            
             
    
    

class PorterStemmer:

    def __init__(self):
        """The main part of the stemming algorithm starts here.
        b is a buffer holding a word to be stemmed. The letters are in b[k0],
        b[k0+1] ... ending at b[k]. In fact k0 = 0 in this demo program. k is
        readjusted downwards as the stemming progresses. Zero termination is
        not in fact used in the algorithm.

        Note that only lower case sequences are stemmed. Forcing to lower case
        should be done before stem(...) is called.
        """

        self.b = ""  # buffer for word to be stemmed
        self.k = 0
        self.k0 = 0
        self.j = 0   # j is a general offset into the string

    def cons(self, i):
        """cons(i) is TRUE <=> b[i] is a consonant."""
        if self.b[i] == 'a' or self.b[i] == 'e' or self.b[i] == 'i' or self.b[i] == 'o' or self.b[i] == 'u':
            return 0
        if self.b[i] == 'y':
            if i == self.k0:
                return 1
            else:
                return (not self.cons(i - 1))
        return 1

    def m(self):
        """m() measures the number of consonant sequences between k0 and j.
        if c is a consonant sequence and v a vowel sequence, and <..>
        indicates arbitrary presence,

           <c><v>       gives 0
           <c>vc<v>     gives 1
           <c>vcvc<v>   gives 2
           <c>vcvcvc<v> gives 3
           ....
        """
        n = 0
        i = self.k0
        while 1:
            if i > self.j:
                return n
            if not self.cons(i):
                break
            i = i + 1
        i = i + 1
        while 1:
            while 1:
                if i > self.j:
                    return n
                if self.cons(i):
                    break
                i = i + 1
            i = i + 1
            n = n + 1
            while 1:
                if i > self.j:
                    return n
                if not self.cons(i):
                    break
                i = i + 1
            i = i + 1

    def vowelinstem(self):
        """vowelinstem() is TRUE <=> k0,...j contains a vowel"""
        for i in range(self.k0, self.j + 1):
            if not self.cons(i):
                return 1
        return 0

    def doublec(self, j):
        """doublec(j) is TRUE <=> j,(j-1) contain a double consonant."""
        if j < (self.k0 + 1):
            return 0
        if (self.b[j] != self.b[j-1]):
            return 0
        return self.cons(j)

    def cvc(self, i):
        """cvc(i) is TRUE <=> i-2,i-1,i has the form consonant - vowel - consonant
        and also if the second c is not w,x or y. this is used when trying to
        restore an e at the end of a short  e.g.

           cav(e), lov(e), hop(e), crim(e), but
           snow, box, tray.
        """
        if i < (self.k0 + 2) or not self.cons(i) or self.cons(i-1) or not self.cons(i-2):
            return 0
        ch = self.b[i]
        if ch == 'w' or ch == 'x' or ch == 'y':
            return 0
        return 1

    def ends(self, s):
        """ends(s) is TRUE <=> k0,...k ends with the string s."""
        length = len(s)
        if s[length - 1] != self.b[self.k]: # tiny speed-up
            return 0
        if length > (self.k - self.k0 + 1):
            return 0
        if self.b[self.k-length+1:self.k+1] != s:
            return 0
        self.j = self.k - length
        return 1

    def setto(self, s):
        """setto(s) sets (j+1),...k to the characters in the string s, readjusting k."""
        length = len(s)
        self.b = self.b[:self.j+1] + s + self.b[self.j+length+1:]
        self.k = self.j + length

    def r(self, s):
        """r(s) is used further down."""
        if self.m() > 0:
            self.setto(s)

    def step1ab(self):
        """step1ab() gets rid of plurals and -ed or -ing. e.g.

           caresses  ->  caress
           ponies    ->  poni
           ties      ->  ti
           caress    ->  caress
           cats      ->  cat

           feed      ->  feed
           agreed    ->  agree
           disabled  ->  disable

           matting   ->  mat
           mating    ->  mate
           meeting   ->  meet
           milling   ->  mill
           messing   ->  mess

           meetings  ->  meet
        """
        if self.b[self.k] == 's':
            if self.ends("sses"):
                self.k = self.k - 2
            elif self.ends("ies"):
                self.setto("i")
            elif self.b[self.k - 1] != 's':
                self.k = self.k - 1
        if self.ends("eed"):
            if self.m() > 0:
                self.k = self.k - 1
        elif (self.ends("ed") or self.ends("ing")) and self.vowelinstem():
            self.k = self.j
            if self.ends("at"):   self.setto("ate")
            elif self.ends("bl"): self.setto("ble")
            elif self.ends("iz"): self.setto("ize")
            elif self.doublec(self.k):
                self.k = self.k - 1
                ch = self.b[self.k]
                if ch == 'l' or ch == 's' or ch == 'z':
                    self.k = self.k + 1
            elif (self.m() == 1 and self.cvc(self.k)):
                self.setto("e")

    def step1c(self):
        """step1c() turns terminal y to i when there is another vowel in the stem."""
        if (self.ends("y") and self.vowelinstem()):
            self.b = self.b[:self.k] + 'i' + self.b[self.k+1:]

    def step2(self):
        """step2() maps double suffices to single ones.
        so -ization ( = -ize plus -ation) maps to -ize etc. note that the
        string before the suffix must give m() > 0.
        """
        if self.b[self.k - 1] == 'a':
            if self.ends("ational"):   self.r("ate")
            elif self.ends("tional"):  self.r("tion")
        elif self.b[self.k - 1] == 'c':
            if self.ends("enci"):      self.r("ence")
            elif self.ends("anci"):    self.r("ance")
        elif self.b[self.k - 1] == 'e':
            if self.ends("izer"):      self.r("ize")
        elif self.b[self.k - 1] == 'l':
            if self.ends("bli"):       self.r("ble") # --DEPARTURE--
            # To match the published algorithm, replace this phrase with
            #   if self.ends("abli"):      self.r("able")
            elif self.ends("alli"):    self.r("al")
            elif self.ends("entli"):   self.r("ent")
            elif self.ends("eli"):     self.r("e")
            elif self.ends("ousli"):   self.r("ous")
        elif self.b[self.k - 1] == 'o':
            if self.ends("ization"):   self.r("ize")
            elif self.ends("ation"):   self.r("ate")
            elif self.ends("ator"):    self.r("ate")
        elif self.b[self.k - 1] == 's':
            if self.ends("alism"):     self.r("al")
            elif self.ends("iveness"): self.r("ive")
            elif self.ends("fulness"): self.r("ful")
            elif self.ends("ousness"): self.r("ous")
        elif self.b[self.k - 1] == 't':
            if self.ends("aliti"):     self.r("al")
            elif self.ends("iviti"):   self.r("ive")
            elif self.ends("biliti"):  self.r("ble")
        elif self.b[self.k - 1] == 'g': # --DEPARTURE--
            if self.ends("logi"):      self.r("log")
        # To match the published algorithm, delete this phrase

    def step3(self):
        """step3() dels with -ic-, -full, -ness etc. similar strategy to step2."""
        if self.b[self.k] == 'e':
            if self.ends("icate"):     self.r("ic")
            elif self.ends("ative"):   self.r("")
            elif self.ends("alize"):   self.r("al")
        elif self.b[self.k] == 'i':
            if self.ends("iciti"):     self.r("ic")
        elif self.b[self.k] == 'l':
            if self.ends("ical"):      self.r("ic")
            elif self.ends("ful"):     self.r("")
        elif self.b[self.k] == 's':
            if self.ends("ness"):      self.r("")

    def step4(self):
        """step4() takes off -ant, -ence etc., in context <c>vcvc<v>."""
        if self.b[self.k - 1] == 'a':
            if self.ends("al"): pass
            else: return
        elif self.b[self.k - 1] == 'c':
            if self.ends("ance"): pass
            elif self.ends("ence"): pass
            else: return
        elif self.b[self.k - 1] == 'e':
            if self.ends("er"): pass
            else: return
        elif self.b[self.k - 1] == 'i':
            if self.ends("ic"): pass
            else: return
        elif self.b[self.k - 1] == 'l':
            if self.ends("able"): pass
            elif self.ends("ible"): pass
            else: return
        elif self.b[self.k - 1] == 'n':
            if self.ends("ant"): pass
            elif self.ends("ement"): pass
            elif self.ends("ment"): pass
            elif self.ends("ent"): pass
            else: return
        elif self.b[self.k - 1] == 'o':
            if self.ends("ion") and (self.b[self.j] == 's' or self.b[self.j] == 't'): pass
            elif self.ends("ou"): pass
            # takes care of -ous
            else: return
        elif self.b[self.k - 1] == 's':
            if self.ends("ism"): pass
            else: return
        elif self.b[self.k - 1] == 't':
            if self.ends("ate"): pass
            elif self.ends("iti"): pass
            else: return
        elif self.b[self.k - 1] == 'u':
            if self.ends("ous"): pass
            else: return
        elif self.b[self.k - 1] == 'v':
            if self.ends("ive"): pass
            else: return
        elif self.b[self.k - 1] == 'z':
            if self.ends("ize"): pass
            else: return
        else:
            return
        if self.m() > 1:
            self.k = self.j

    def step5(self):
        """step5() removes a final -e if m() > 1, and changes -ll to -l if
        m() > 1.
        """
        self.j = self.k
        if self.b[self.k] == 'e':
            a = self.m()
            if a > 1 or (a == 1 and not self.cvc(self.k-1)):
                self.k = self.k - 1
        if self.b[self.k] == 'l' and self.doublec(self.k) and self.m() > 1:
            self.k = self.k -1

    def stem(self, p, i, j):
        """In stem(p,i,j), p is a char pointer, and the string to be stemmed
        is from p[i] to p[j] inclusive. Typically i is zero and j is the
        offset to the last character of a string, (p[j+1] == '\0'). The
        stemmer adjusts the characters p[i] ... p[j] and returns the new
        end-point of the string, k. Stemming never increases word length, so
        i <= k <= j. To turn the stemmer into a module, declare 'stem' as
        extern, and delete the remainder of this file.
        """
        # copy the parameters into statics
        self.b = p
        self.k = j
        self.k0 = i
        if self.k <= self.k0 + 1:
            return self.b # --DEPARTURE--

        # With this line, strings of length 1 or 2 don't go through the
        # stemming process, although no mention is made of this in the
        # published algorithm. Remove the line to match the published
        # algorithm.

        self.step1ab()
        self.step1c()
        self.step2()
        self.step3()
        self.step4()
        self.step5()
        return self.b[self.k0:self.k+1]
    




if __name__ == "__main__":
    main()

    