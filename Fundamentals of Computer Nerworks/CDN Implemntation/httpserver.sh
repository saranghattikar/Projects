# CS 5700: Project 5  Team : "mjnsh"

port=
origin=

#Check if the number of arguments are not less than 2  
#if [[ "$#" -lt 2 ]] ; then
#   echo "Error: illegal number of parameters"
#   exit 2	
#fi


while getopts ":p:o:" optname; do
  
     case "${optname}" in
       p)
          
	  port=${OPTARG}
        
	  ;;
       o)
          origin=${OPTARG}l
           
          ;;
       *) 
          echo "Usage: $0 -p <port> -o <origin> "
          ;;
       
     esac
done

regex='^[0-9]+$'

if ! [[ ${port} =~ $regex ]] ; then 
  	echo "Error : invalid port number "
  	 exit 2
fi

# Function usage specifies the usage of the command in case of an error
#usage() {
#    echo "Usage: $0 -p <port> -o <origin> "
#}

python httpserver.py ${port} ${origin}


