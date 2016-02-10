# CS 5700: Project 5  Team : "mjnsh"

port=
name=

#Check if the number of arguments are not less than 2  
#if [[ "$#" -lt 2 ]] ; then
#   echo "Error: illegal number of parameters"
#   exit 2	
#fi


while getopts ":p:n:" optname; do
  
     case "${optname}" in
       p)
          
	  port=${OPTARG}
        
	  ;;
       n)
          name=${OPTARG}
           
          ;;
       *) 
           usage
          ;;
       
     esac
done

regex='^[0-9]+$'

if ! [[ ${port} =~ $regex ]] ; then 
  "Error : invalid port number"
  exit 2
fi

# Function usage specifies the usage of the command in case of an error
usage() {
    echo "Usage: ./dnsserver -p <port> -n <name> "
}

python DNS.py ${port} ${name}


