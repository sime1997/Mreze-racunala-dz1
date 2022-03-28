#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "protocol.h"


int obradiLOGIN( int sock, const char *ime, const char *poruka );
void obradiBYE( int sock, const char *ime, const char *poruka );
int obradiUSERS( int sock, const char *ime, const char *poruka );
int obradiCHECKFILES( int sock, const char *ime, const char *poruka );
int obradiSENDFILE( int sock, const char *ime, const char *poruka );
int obradiRECEIVEFILE( int sock, const char *ime, const char *poruka );

#define USERLIST_FILENAME       "file.userlist"

#define MAXDRETVI 3

typedef struct
{
	int commSocket;
	int indexDretve;
} obradiKlijenta__parametar;

int aktivneDretve[MAXDRETVI] = { 0 };
obradiKlijenta__parametar parametarDretve[MAXDRETVI];
pthread_mutex_t lokot_aktivneDretve = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t lokot_filesystem = PTHREAD_MUTEX_INITIALIZER;


void krajKomunikacije( void *parametar, const char *ime )
{
	obradiKlijenta__parametar *param = (obradiKlijenta__parametar *) parametar;
	int commSocket  = param->commSocket;
	int indexDretve = param->indexDretve;

	printf( "Kraj komunikacije [dretva=%d, ime=%s]... \n", indexDretve, ime );

	pthread_mutex_lock( &lokot_aktivneDretve );
	aktivneDretve[ indexDretve ] = 2;
	pthread_mutex_unlock( &lokot_aktivneDretve );

	close( commSocket );
}

void *obradiKlijenta( void *parametar )
{
	obradiKlijenta__parametar *param = (obradiKlijenta__parametar *) parametar;
	int commSocket  = param->commSocket;

	int vrstaPoruke;
	char *poruka;

	// prvo trazi login
	if( primiPoruku( commSocket, &vrstaPoruke, &poruka ) != OK )
	{
		krajKomunikacije( parametar, "" );
		return NULL;
	}

	if( vrstaPoruke != LOGIN || strlen( poruka ) > 8 )
	{
		krajKomunikacije( parametar, "" );
		return NULL;
	}

	char imeKlijenta[9];
	strcpy( imeKlijenta, poruka );
	if( obradiLOGIN( commSocket, imeKlijenta, poruka ) != OK )
	{
		krajKomunikacije( parametar, imeKlijenta );
		return NULL;
	}
	//free( poruka );

	// onda u petlji obradjuj ostale zahtjeve
	int gotovo = 0;
	while( !gotovo )
	{
        if( primiPoruku( commSocket, &vrstaPoruke, &poruka ) == NIJEOK )
        {
			krajKomunikacije( parametar, imeKlijenta );
			gotovo = 1;
			continue;
		}

		switch( vrstaPoruke )
		{
			case BYE:
			    obradiBYE( commSocket, imeKlijenta, poruka );
                krajKomunikacije( parametar, imeKlijenta ); gotovo = 1;
				break;

			case USERS:
				if( obradiUSERS( commSocket, imeKlijenta, poruka ) != OK )
				{
                    krajKomunikacije( parametar, imeKlijenta ); gotovo = 1;
				}
				break;

			case SENDFILE:
				if( obradiSENDFILE( commSocket, imeKlijenta, poruka ) != OK )
				{
                    krajKomunikacije( parametar, imeKlijenta ); gotovo = 1;
				}
				break;

			case CHECKFILES:
				if( obradiCHECKFILES( commSocket, imeKlijenta, poruka ) != OK )
				{
                    krajKomunikacije( parametar, imeKlijenta ); gotovo = 1;
				}
				break;

            case RECEIVEFILE:
				if( obradiRECEIVEFILE( commSocket, imeKlijenta, poruka ) != OK )
				{
                    krajKomunikacije( parametar, imeKlijenta ); gotovo = 1;
				}
				break;

			default: krajKomunikacije( parametar, imeKlijenta ); gotovo = 1; break;
		}

		free( poruka );
	}

	return NULL;
}


int main( int argc, char **argv )
{
	if( argc != 2 )
		error2( "Upotreba: %s port\n", argv[0] );

	int port;
	sscanf( argv[1], "%d", &port );

	// socket...
	int listenerSocket = socket( PF_INET, SOCK_STREAM, 0 );
	if( listenerSocket == -1 )
	    perror( "socket" );

	// bind...
	struct sockaddr_in mojaAdresa;

	mojaAdresa.sin_family      = AF_INET;
	mojaAdresa.sin_port        = htons( port );
	mojaAdresa.sin_addr.s_addr = INADDR_ANY;
	memset( mojaAdresa.sin_zero, '\0', 8 );

	if( bind(
			listenerSocket,
			(struct sockaddr *) &mojaAdresa,
			sizeof( mojaAdresa ) ) == -1 )
		perror( "bind" );

	// listen...
	if( listen( listenerSocket, 10 ) == -1 )
		perror( "listen" );

	pthread_t dretve[10];

	while( 1 ) // vjecno cekamo nove klijente...
	{
		// accept...
		struct sockaddr_in klijentAdresa;
		unsigned int lenAddr = sizeof( klijentAdresa );
		int commSocket = accept( listenerSocket,
                         (struct sockaddr *) &klijentAdresa,
                         &lenAddr );

		if( commSocket == -1 )
			perror( "accept" );

		char *dekadskiIP = inet_ntoa( klijentAdresa.sin_addr );
		printf( "Prihvatio konekciju od %s ", dekadskiIP );

		pthread_mutex_lock( &lokot_aktivneDretve );
		int i, indexNeaktivne = -1;
		for( i = 0; i < MAXDRETVI; ++i )
			if( aktivneDretve[i] == 0 )
				indexNeaktivne = i;
			else if( aktivneDretve[i] == 2 )
			{
				pthread_join( dretve[i], NULL );
				aktivneDretve[i] = 0;
				indexNeaktivne = i;
			}

		if( indexNeaktivne == -1 )
		{
			close( commSocket ); // nemam vise dretvi...
			printf( "--> ipak odbijam konekciju jer nemam vise dretvi.\n" );
		}
		else
		{
		    aktivneDretve[indexNeaktivne] = 1;
			parametarDretve[indexNeaktivne].commSocket = commSocket;
			parametarDretve[indexNeaktivne].indexDretve = indexNeaktivne;
			printf( "--> koristim dretvu broj %d.\n", indexNeaktivne );

			pthread_create(
				&dretve[indexNeaktivne], NULL,
				obradiKlijenta, &parametarDretve[indexNeaktivne] );
		}
		pthread_mutex_unlock( &lokot_aktivneDretve );
	}

	return 0;
}



int obradiLOGIN( int sock, const char *ime, const char *poruka )
{
	// postoji li vec korisnik sa imenom ime?
	// samo 1 dretva smije u neko vrijeme prckati po file-ovima!
	pthread_mutex_lock( &lokot_filesystem );

	FILE *f;
	if( ( f = fopen( USERLIST_FILENAME, "rt" ) ) == NULL )
	{
		// nema jos tog file-a -- samo dodaj ovog korisnika i zatvori file
		if( ( f = fopen( USERLIST_FILENAME, "wt" ) ) == NULL )
		{
            pthread_mutex_unlock( &lokot_filesystem );
		    return NIJEOK;
		}

		fprintf( f, "%s\n", ime );
		fclose( f );
	}
	else
	{
		// provjeri jel ovo ime korisnika vec postoji
		char imeSaPopisa[9];
		int ima = 0;
		while( fscanf( f, "%s", imeSaPopisa ) == 1 && !ima )
			if( strcmp( imeSaPopisa, ime ) == 0 )
			    ima = 1;

		if( !ima )
		{
			// znaci, nema tog imena pa ga mozemo dodati
			fclose( f );
			if( ( f = fopen( USERLIST_FILENAME, "at" ) ) == NULL )
			{
            	pthread_mutex_unlock( &lokot_filesystem );
		    	return NIJEOK;
			}

			fprintf( f, "%s\n", ime );
			fclose( f );
		}
		else
        {
            printf( "Korisnicko ime %s vec postoji.", ime );
            fclose( f );
            pthread_mutex_unlock( &lokot_filesystem );
		    return NIJEOK;
        }
	}

	pthread_mutex_unlock( &lokot_filesystem );
	return OK;
}

void obradiBYE( int sock, const char *ime, const char *poruka )
{
    pthread_mutex_lock( &lokot_filesystem );

	FILE *f;
	FILE *fp;
	f = fopen( USERLIST_FILENAME, "rt" );
	fp = fopen( "temp.txt", "wt" );

	char imeSaPopisa[9];
	char c = '\n';

    while( fscanf( f, "%s", imeSaPopisa ) == 1 )
    {
         if( strcmp( imeSaPopisa, ime ) != 0 )
         {
             fprintf( fp, "%s", imeSaPopisa );
             fputc( c, fp );
         }
    }

    remove( USERLIST_FILENAME );

    rename( "temp.txt", USERLIST_FILENAME );

    fclose( fp );
    fclose( f );

    pthread_mutex_unlock( &lokot_filesystem );
}


int obradiUSERS( int sock, const char *ime, const char *poruka )
{
	if( strlen( poruka ) != 0 )
	    return NIJEOK;

	// samo 1 dretva smije u neko vrijeme prckati po file-ovima!
	pthread_mutex_lock( &lokot_filesystem );

	FILE *f;
	if( ( f = fopen( USERLIST_FILENAME, "rt" ) ) == NULL )
	{
        pthread_mutex_unlock( &lokot_filesystem );
        return NIJEOK;
	}

	char *zaSlanje = NULL, imeSaPopisa[9];
	int duljinaZaSlanje = 0;
	while( fscanf( f, "%s", imeSaPopisa ) == 1 )
	{
		if( zaSlanje == NULL )
		{
			zaSlanje = (char *) malloc( duljinaZaSlanje + 1 + strlen( imeSaPopisa ) );
			strcpy( zaSlanje + duljinaZaSlanje, imeSaPopisa );
			duljinaZaSlanje += strlen( imeSaPopisa );
		}
		else
		{
			zaSlanje = (char *) realloc( zaSlanje, duljinaZaSlanje + 2 + strlen( imeSaPopisa ) );
			strcpy( zaSlanje + duljinaZaSlanje, " " );
			++duljinaZaSlanje;
			strcpy( zaSlanje + duljinaZaSlanje, imeSaPopisa ); duljinaZaSlanje += strlen( imeSaPopisa );
		}
	}
	pthread_mutex_unlock( &lokot_filesystem );

	return posaljiPoruku( sock, USERLIST, zaSlanje );
}

int obradiCHECKFILES( int sock, const char *ime, const char *poruka )
{
	if( strlen( poruka ) != 0 )
		return NIJEOK;

	FILE *f;
	char fileName[20] = "file.";
	strcat( fileName, ime );
	strcat( fileName, ".txt" );

	char zaSlanje[1000];

    pthread_mutex_lock( &lokot_filesystem );

    if( ( f = fopen( fileName, "rt" ) ) == NULL )
    {
        pthread_mutex_unlock( &lokot_filesystem );
        return posaljiPoruku( sock, FILELIST, "" );
    }

    if( procitajDatoteku( f, zaSlanje , sizeof( zaSlanje )) != 1 )
    {
        fclose( f );
        pthread_mutex_unlock( &lokot_filesystem );
        return NIJEOK;
    }

    pthread_mutex_unlock( &lokot_filesystem );
    return posaljiPoruku( sock, FILELIST, zaSlanje);

}

int obradiSENDFILE( int sock, const char *ime, const char *poruka )
{
    if( strlen( poruka ) == 0 )
	    return NIJEOK;

    // skuzi kome je namjenjena datoteka
	char kome[9];
	sscanf( poruka, "%s", kome );
	poruka += strlen( kome ) + 1;

	//kako se zove datoteka
	char datoteka[100];
	char temp[100];
	sscanf( poruka, "%s", temp );
	strcpy( datoteka, "s" );
	strcat( datoteka, temp );
	poruka += strlen( datoteka ) ;

	// otvori datoteku u koju upisujemo ime dolaznih datoteka za svakog klijenta
	FILE *f;
	char fileName[100] = "file.";
	strcat( fileName, kome );
	strcat( fileName, ".txt" );

	//spremi datoteku koju je poslao klijent
	FILE *fp;
	char buf[1000];
	strcpy( buf, poruka );

	pthread_mutex_lock( &lokot_filesystem );
	if( ( f = fopen( fileName, "at" ) ) != NULL ) // dopisivanje na kraj
	{
		fprintf( f, "%s\n", datoteka );
		fclose( f ) ;
		if( ( fp = fopen( datoteka, "wt" ) ) != NULL )
        {
            if( upisiDatoteku( fp, buf, strlen( buf ) ) != 1 )
            {
                pthread_mutex_unlock( &lokot_filesystem );
                return NIJEOK;
            }
            fclose( fp );
            buf[0] = '\0';
        }
        else
        {
            pthread_mutex_unlock( &lokot_filesystem );
            return NIJEOK;
        }
	}
	else
	{
		pthread_mutex_unlock( &lokot_filesystem );
		return NIJEOK;
	}

	pthread_mutex_unlock( &lokot_filesystem );
	return OK;

}

int obradiRECEIVEFILE( int sock, const char *ime, const char *poruka )
{
    if ( strlen( poruka ) != 0)
        return NIJEOK;

    FILE *fp;

    char fileName[20] = "file.";
    strcat( fileName, ime );
    strcat( fileName, ".txt" );

    char zaSlanje[1000];

    pthread_mutex_lock( &lokot_filesystem );

    if( ( fp = fopen( fileName, "rt") ) == NULL)
    {
        pthread_mutex_unlock( &lokot_filesystem );
        return NIJEOK;
    }

    char imeDatoteke[20];
    char datoteka[20];
    fscanf( fp, "%s", imeDatoteke );

    FILE *ft;

    ft = fopen( "temp.txt", "wt" );

    while( fscanf( fp, "%s", datoteka ) == 1)
    {
        fprintf( ft, "%s", datoteka );
        //fputc( '\n', ft );
    }

    remove( fileName );
    rename( "temp.txt", fileName );

    fclose( fp );
    fclose( ft );

    strcpy( zaSlanje, "k2" );
    strcat( zaSlanje, imeDatoteke);
    strcat( zaSlanje, " " );

    FILE *f;

    if( ( f = fopen( imeDatoteke, "rt" ) ) == NULL )
    {
        pthread_mutex_unlock( &lokot_filesystem );
        return NIJEOK;
    }

    char sadrzaj[1000];

    if( procitajDatoteku( f, sadrzaj, sizeof(sadrzaj) ) != 1 )
    {
        pthread_mutex_unlock( &lokot_filesystem );
        return NIJEOK;
    }

    strcat( zaSlanje, sadrzaj );

    fclose( f );

    remove( imeDatoteke );

    pthread_mutex_unlock( &lokot_filesystem );

	return posaljiPoruku( sock, FFILE, zaSlanje );

}


