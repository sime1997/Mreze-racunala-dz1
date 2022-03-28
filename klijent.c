#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "protocol.h"

void obradiLOGIN( int sock, const char *ime );
void obradiBYE( int sock, const char *ime );
void obradiUSERS( int sock, const char *ime );
void obradiCHECKFILES( int sock, const char *ime );
void obradiSENDFILE( int sock, const char *ime );
void obradiRECEIVEFILE( int sock, const char *ime );



int main( int argc, char **argv)
{
    if( argc != 4 )
        error2( "Upotreba: %s ime ip port\n", argv[0] );

    char mojeIme[9];
    strcpy( mojeIme, argv[1]);
    mojeIme[strlen(mojeIme)]='\0';

    char *dekadskiIP = argv[2];
	int port;
	sscanf( argv[3], "%d", &port );

	// socket
	int mojSocket = socket( PF_INET, SOCK_STREAM, 0 );
	if( mojSocket == -1 )
		perror( "socket: " );

    // connect...
	struct sockaddr_in adresaServera;

	adresaServera.sin_family = AF_INET;
	adresaServera.sin_port = htons( port );

	if( inet_aton( dekadskiIP, &adresaServera.sin_addr ) == 0 )
		error2( "%s nije dobra adresa!\n", dekadskiIP );

	memset( adresaServera.sin_zero, '\0', 8 );

	if( connect( mojSocket, (struct sockaddr *) &adresaServera, sizeof( adresaServera ) ) == -1 )
		perror( "connect: " );

	obradiLOGIN( mojSocket, mojeIme );

	int gotovo = 0;
	while( !gotovo )
	{
		printf( "\n\nSto zelite, %s?\n", mojeIme );
		printf( "\t0. izlaz\n" );
		printf( "\t1. pogledati korisnička imena trenutno ulogiranih klijenata\n" );
		printf( "\t2. poslati lokalnu datoteku\n" );
		printf( "\t3. provjeriti popis dolaznih datoteka\n" );
		printf( "\t4. primiti dolaznu datoteku\n\n: " );

	  int izbor;
		scanf( "%d", &izbor );

		switch( izbor )
		{
			case 0: obradiBYE( mojSocket, mojeIme ); gotovo = 1; break;
			case 1: obradiUSERS( mojSocket, mojeIme ); break;
			case 2: obradiSENDFILE( mojSocket, mojeIme ); break;
			case 3: obradiCHECKFILES( mojSocket, mojeIme ); break;
			case 4: obradiRECEIVEFILE( mojSocket, mojeIme ); break;
			default: printf( "Ponovno odaberite sto zelite.. " );
		}
	}

	return 0;
}


void obradiLOGIN( int sock, const char *ime )
{
	if( posaljiPoruku( sock, LOGIN, ime ) == NIJEOK )
		error1( "Pogreska u LOGIN...izlazim.\n" );
}


void obradiBYE( int sock, const char *ime )
{
	if( posaljiPoruku( sock, BYE, "" ) == NIJEOK )
		error1( "Pogreska u BYE...izlazim.\n" );

	close( sock );
}

void obradiUSERS( int sock, const char *ime )
{
    if( posaljiPoruku( sock, USERS, "" ) == NIJEOK )
		error1( "Pogreska u USERS...izlazim.\n" );

	int vrstaPoruke;
	char *popisKorisnika;

	if( primiPoruku( sock, &vrstaPoruke, &popisKorisnika ) == NIJEOK )
		error1( "Pogreska u USERS...izlazim.\n" );

	if( vrstaPoruke != USERLIST )
		error1( "Pogreska u USERS...izlazim.\n" );

	printf( "Popis svih trenutno ulogiranih korisnika servera: %s\n", popisKorisnika );

	free( popisKorisnika );
}

void obradiSENDFILE( int sock, const char *ime )
{
    char *poruka = (char *) malloc( 9 * sizeof( char ) );
    char datoteka[20];
    char sadrzaj[1000];
    FILE *fp;

    printf( "Korisnicko ime klijenta kome zelite poslati datoteku: " );
    scanf( " %8[^\n]", poruka );
    fflush( stdin ); // ignoriraj sve sto je uneseno iza 8. znaka

    printf( "Koju datoteku zelite poslati? " );
    scanf( "%s", datoteka);
    int duljinaPoruke = strlen( poruka );
    poruka = (char *) realloc( poruka, duljinaPoruke + strlen( datoteka ) + 2 );
	strcpy( poruka + duljinaPoruke, "\n" );
	++duljinaPoruke;
	strcpy( poruka + duljinaPoruke, datoteka );
	duljinaPoruke += strlen( datoteka );

	fp = fopen( datoteka, "rt" );

	if( fp == NULL)
        printf( "Greska u otvaranju datoteke" );


	if( procitajDatoteku( fp, sadrzaj, sizeof( sadrzaj ) ) )
    {
        poruka = ( char *) realloc( poruka, duljinaPoruke + strlen( sadrzaj ) + 2 );
        strcpy( poruka + duljinaPoruke , "\n" );
        ++duljinaPoruke;
        strcpy( poruka + duljinaPoruke , sadrzaj );
        duljinaPoruke += strlen( sadrzaj );
    }

    //remove( datoteka );

    if (fp != NULL)
            fclose(fp);

    if( posaljiPoruku( sock, SENDFILE, poruka ) == NIJEOK )
		error1( "Pogreska u SENDFILE...izlazim.\n" );

	free( poruka );
}

void obradiCHECKFILES( int sock, const char *ime )
{
    if( posaljiPoruku( sock, CHECKFILES, "" ) == NIJEOK )
		error1( "Pogreska u CHECKFILES...izlazim.\n" );

    int vrstaPoruke;
	char *poruka, *origPoruka;

	if( primiPoruku( sock, &vrstaPoruke, &origPoruka ) == NIJEOK )
		error1( "Pogreska u CHECKFILES...izlazim.\n" );

    poruka = origPoruka;

	if( vrstaPoruke != FILELIST )
		error1( "Pogreska u CHECKFILES...izlazim.\n" );

    if( strcmp( poruka, "" ) == 0 )
	{
		printf( "Nema dolaznih datoteka.\n" );
		return;
	}

    printf( "%s", poruka );

	//free( origPoruka );
}

void obradiRECEIVEFILE( int sock, const char *ime )
{
    FILE *fp;

    if( posaljiPoruku( sock, RECEIVEFILE, "" ) == NIJEOK )
		error1( "Pogreska u RECEIVEFILE...izlazim.\n" );

    int vrstaPoruke;
	char *poruka, *origPoruka;

	if( primiPoruku( sock, &vrstaPoruke, &origPoruka ) == NIJEOK )
		error1( "Pogreska u RECEIVEFILE...izlazim.\n" );

     poruka = origPoruka;

	if( vrstaPoruke != FFILE )
		error1( "Pogreska u RECEIVEFILES...izlazim.\n" );

    char fileName[20];
    sscanf( poruka, "%s", fileName );

    if( ( fp = fopen( fileName, "wt") ) == NULL )
        error1( "Pogreska u RECEIVEFILES...izlazim.\n" );

    poruka += strlen( fileName );
    poruka++;

    if( upisiDatoteku( fp, poruka, strlen( poruka ) ) != 1  )
       error1( "Pogreska u RECEIVEFILES...izlazim.\n" );

    fclose( fp );

    printf( "Datoteka uspjesno primljena." );

}
