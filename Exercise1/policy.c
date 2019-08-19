#include <errno .h> 
#include <stdlib .h> 
#include <stdio .h> 
#include <string .h>

int main( int argc , char*argv []) { 
	FILE *f ;
	char line [1024] , pol [10] , temp_name[40] , temp_val [10];
	char **names=NULL;
	char ** values=NULL;
	char * p;
	int n_spaces=0,v_spaces=0, i ;
	int sum_cpu=0;
	float score ;
	char c;
	f = stdin ;
	while (1) {
		if ( fscanf (f , " policy :") <0) break ;
		i =0;
		c=fgetc ( f ) ;
		
		while(c != ': ' ) { 
			temp_name[ i]=c;
			i ++; c=fgetc ( f ) ; 
		
		} 
	
		temp_name[ i ]= '\0 ';
		names =(char **) realloc (names , sizeof (char *) * (++ n_spaces ) ) ;
		names[n_spaces−1]=malloc ( sizeof (char) *( strlen ( temp_name)+1)) ;
		strncpy (names[n_spaces −1],temp_name , strlen (temp_name) ) ;
		fgetc ( f ) ;
		fgetc ( f ) ;
		fgetc ( f ) ;
		fgetc ( f ) ;
		i =0;
		c=fgetc ( f ) ;
		
		while(c!= '\n' && c!=EOF) { 
			temp_val[ i]=c; i ++;
			c=fgetc ( f ) ;
		} 
		
		temp_val[ i ]= '\0 ';
		values = (char **) realloc ( values , sizeof (char *) * (++ v_spaces ) ) ;
		values[v_spaces−1]=malloc ( sizeof (char) *( strlen ( temp_val)+1)) ;
		strncpy ( values[v_spaces −1],temp_val , strlen (temp_val) ) ;
		sum_cpu=sum_cpu+( int ) strtol ( values[v_spaces −1],(char **)NULL,10) ;
	}
	
	score=sum_cpu /2000.0;
	
	if (score >1) score=−score ;
	printf (" score :%.2 f \n" , score ) ;
	for ( i =0; i <(n_spaces ) ; i++)
		printf (" set_limit :%s :cpu. shares:%d\n" , names[ i ] , ( int ) strtol ( values[ i ] , (char **)NULL, 10) ) ;
	
	free ( values ) ; free (names) ;
	return 0;
	
} 