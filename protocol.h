#ifndef __FILEPROTOCOL_H_
#define __FILEPROTOCOL_H_

#define LOGIN       	1
#define USERS       	2
#define USERLIST    	3
#define BYE         	4
#define SENDFILE        5
#define CHECKFILES      6
#define FILELIST        7
#define RECEIVEFILE     8
#define FFILE           9


// ovo ispod koriste i klijent i server, pa moze biti tu...
#define OK      1
#define NIJEOK  0

int primiPoruku( int sock, int *vrstaPoruke, char **poruka );
int posaljiPoruku( int sock, int vrstaPoruke, const char *poruka );

int procitajDatoteku( FILE *fp, char *buf, int s );
int upisiDatoteku( FILE *fp, char *buf, int s );

#define error1( s ) { printf( s ); exit( 0 ); }
#define error2( s1, s2 ) { printf( s1, s2 ); exit( 0 ); }
#define myperror( s ) { perror( s ); exit( 0 ); }

#endif
