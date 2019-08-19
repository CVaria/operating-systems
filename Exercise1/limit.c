#include <stdio .h>
#include <stdlib .h> 
#include <string .h> 
#define PATH "/ sys/ fs /cgroup/cpu/" char temp[100]; 

int next_line () { 
	int i =0;
	char c;
	c=getchar () ;
	for (; c!=EOF && c!= '\n '; c=getchar () , i++)
		temp[ i]=c; 
	temp[ i ]= '\0 ';
	return i ; 
} 

int next_word( const char temp[] , char arr [] , int index){
	int i=index , newi=0;
	for (; temp[ i ]!= ': ' && temp[ i ]!= '\0 '; i++, newi++) 
		arr[newi]=temp[ i ];

	arr[newi]= '\0 ';
	return newi ;// length of word
} 

int main () { 
	int i , len , act_len , mon_len , app_len , count ;
	char action [10] , monitor[20] , app_name[40] , pid [5] , value [5] , command[100] , total_path [200];

	while( next_line () !=0) { 
		command[0]= '\0 ';
		total_path [0]= '\0 '; /* Parse word*/ 
		len=strlen (temp) ;
		count=0;
		count+= next_word(temp , action , 0) ;
		count+= next_word(temp , monitor , count+1) ;
		count +=6;
		//add ": cpu :"
		count+= next_word(temp , app_name , count) ;
		strcat ( total_path , PATH) ;
		strcat ( total_path , monitor) ;
		strcat ( total_path , "/") ;
		strcat ( total_path , app_name) ;
		strcat ( total_path , "/") ;

		if (! strcmp( action ," add") ) {
			//more arguements 
			count+= next_word(temp , pid , count+1) ;
			/*add an process into existing cgroup*/ 
			strcat (command, "echo ") ;
			strcat (command, pid ) ;
			strcat (command, " >> ") ;
			strcat (command, total_path ) ;
			strcat (command, " tasks ") ;
			system(command) ; 
		} else if (! strcmp( action , " set_limit ") ) { 
			/*Change cpu. shares for app_name*/ 
			count+= next_word(temp , value , count+12) ; 
			//: cpu. shares : 
			strcat (command, "echo ") ;
			strcat (command, value ) ;
			strcat (command, "> ") ;
			strcat (command, total_path ) ;
			strcat (command, "cpu. shares ") ;
			system(command) ; 
		} else if (! strcmp( action , " create ") ) { 
			/* create directory−cgroup*/
			strcat (command," mkdir −p ") ;
			strcat (command, total_path ) ;
			system(command) ; 
		} else { 
			/*Remove directory−cgroup*/
			strcat (command," rmdir ") ; /// sys/ fs /cgroup /") ;
			strcat (command, total_path ) ;
			system(command) ; 
		}	
	} 
	
	return 0;
} 