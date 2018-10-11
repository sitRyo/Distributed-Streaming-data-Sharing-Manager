#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>

#include "ssm.h"
#include "libssm.h"

int gIsVerbose = 1;

int init(  )
{
	static int isInit = 0;
	if( !isInit )
	{
		if( !initSSM(  ) )
		{
			fprintf( stderr, "ERROR : cannot init ssm.\n" );
			return 0;
		}
		isInit = 1;
	}
	return 1;
}

void printParam( int opt )
{
	if( opt == 'A' || opt == 'a' || opt == 'd' )
	{
		char day[64];
		time_t t = gettimeSSM(  );
		strftime( day, sizeof ( day ), "%Y/%m/%d(%a) %H:%M:%S %Z", localtime( &t ) );
		printf( "%s%s\n", ( gIsVerbose ? "SSM DATE   : " : "" ), day );
		// printf("%s%lf\n",(gIsVerbose ? "SSMTIME : " : ""), gettimeSSM());
	}

	if( opt == 'A' || opt == 'a' || opt == 't' )
		printf( "%s%lf\n", ( gIsVerbose ? "SSM TIME   : " : "" ), gettimeSSM(  ) );

	if( opt == 'A' || opt == 'u' )
	{
		char day[64];
		time_t t = gettimeSSM_real(  );
		strftime( day, sizeof ( day ), "%Y/%m/%d(%a) %H:%M:%S %Z", localtime( &t ) );
		printf( "%s%s\n", ( gIsVerbose ? "REAL DATE  : " : "" ), day );
		// printf("%s%lf\n",(gIsVerbose ? "SSMTIME : " : ""), gettimeSSM());
	}

	if( opt == 'A' || opt == 'U' )
		printf( "%s%lf\n", ( gIsVerbose ? "REAL TIME  : " : "" ), gettimeSSM_real(  ) );

	if( opt == 'A' || opt == 'a' || opt == 'x' )
		printf( "%s%lf\n", ( gIsVerbose ? "SSM SPEED  : " : "" ), gettimeSSM_speed(  ) );

	if( gIsVerbose )
	{
		if( opt == 'A' || opt == 'a' || opt == 'p' )
			printf( "%s%s\n", "IS_PAUSE   : ", ( gettimeSSM_is_pause(  )? "PAUSE" : "PLAY" ) );
		if( opt == 'A' || opt == 'r' )
			printf( "%s%s\n", "IS_REVERSE : ", ( gettimeSSM_is_reverse(  )? "REVERSE" : "FORWARD" ) );
	}
	else
	{
		if( opt == 'A' || opt == 'a' || opt == 'p' )
			printf( "%d\n", ( gettimeSSM_is_pause(  )? 1 : 0 ) );
		if( opt == 'A' || opt == 'r' )
			printf( "%d\n", ( gettimeSSM_is_reverse(  )? 1 : 0 ) );
	}
}

void printHelp( const char *name, int isLongHelp )
{
	fprintf( stderr, "HELP\n" );
	// fprintf(stderr,"\t- | -- :\n");
	fprintf( stderr, "\t-a | --get-all         : print all ssm time property\n" );
	fprintf( stderr, "\t-A | --get-full        : print full ssm time property\n" );
	fprintf( stderr, "\t-I | --init            : reset ssm time.\n" );
	fprintf( stderr, "\t-s | --increase-speed  : increase speed a little.\n" );
	fprintf( stderr, "\t-S | --decrease-speed  : decrease speed a little.\n" );
	fprintf( stderr, "\t-g | --toggle-pause    : toggle pause state.\n" );
	fprintf( stderr, "\t-G | --toggle-reverse  : toggle time reverse state.\n" );
	fprintf( stderr, "\t-v | --verbose         : set verbose.\n" );
	fprintf( stderr, "\t-q | --quiet           : set NOT verbose.\n" );
	fprintf( stderr, "\t-h | --help            : print this help.\n" );
	fprintf( stderr, "\t-H | --long-help       : print long help.\n" );
	if( isLongHelp )
	{
		fprintf( stderr, "\nLONG HELP\n" );
		fprintf( stderr, "\t-d | --get-date        : get ssm date.\n" );
		fprintf( stderr, "\t-t | --get-time        : get ssm time.\n" );
		fprintf( stderr, "\t-T | --set-time TIME   : set time to TIME.\n" );
		fprintf( stderr, "\t-u | --get-date-real   : get unix date.\n" );
		fprintf( stderr, "\t-U | --get-time-real   : get unix time.\n" );
		fprintf( stderr, "\t-x | --get-speed       : get speed.\n" );
		fprintf( stderr, "\t-X | --set-speed SPEED : set speed to SPEED.\n" );
		fprintf( stderr, "\t-p | --get-pause       : get pause state.\n" );
		fprintf( stderr, "\t-P | --set-pause PAUSE : set pause state to PAUSE.\n" );
		fprintf( stderr, "\t-r | --get-reverse     : get reverse state.\n" );
		fprintf( stderr, "\t-R | --set-reverse REV : set reverse state to REV.\n" );
		fprintf( stderr, "\t   | --sleep  TIME     : sleep TIME sec.\n" );
		fprintf( stderr, "\t   | --usleep TIME     : usleep TIME usec.\n" );
	}
	fprintf( stderr, "ex)\n\t $ %s\n", name );
}

int main( int aArgc, char **aArgv )
{
	int opt, optIndex = 0, optFlag = 0;
	struct option longOpt[] = {
		{"get-all", 0, 0, 'a'},
		{"get-full", 0, 0, 'A'},
		{"init", 0, 0, 'I'},
		{"get-date", 0, 0, 'd'},
		{"get-time", 0, 0, 't'},
		{"set-time", 1, 0, 'T'},
		{"get-date-real", 0, 0, 'u'},
		{"get-time-real", 0, 0, 'U'},
		{"get-speed", 0, 0, 'x'},
		{"set-speed", 1, 0, 'X'},
		{"increase-speed", 0, 0, 's'},
		{"decrease-speed", 0, 0, 'S'},
		{"get-pause", 0, 0, 'p'},
		{"set-pause", 1, 0, 'P'},
		{"get-reverse", 0, 0, 'r'},
		{"set-reverse", 1, 0, 'R'},
		{"toggle-pause", 0, 0, 'g'},
		{"toggle-reverse", 0, 0, 'G'},
		{"sleep", 1, &optFlag, 's'},
		{"usleep", 1, &optFlag, 'u'},
		{"quiet", 0, 0, 'q'},
		{"verbose", 0, 0, 'v'},
		{"help", 0, 0, 'h'},
		{"long-help", 0, 0, 'H'},
		{0, 0, 0, 0}
	};

	while( ( opt = getopt_long( aArgc, aArgv, "aAIdtT:uUxX:sSpP:rR:gGvqhH", longOpt, &optIndex ) ) != -1 )
	{
		if( opt != 'h' && opt != 'H' && !init(  ) )
		{
			return 1;
		}
		switch ( opt )
		{
		case 'a':
		case 'A':
		case 'd':
		case 't':
		case 'u':
		case 'U':
		case 'x':
		case 'p':
		case 'r':
			printParam( opt );
			break;
		case 'I':
			inittimeSSM(  );
			if( gIsVerbose )
				printParam( 'a' );
			break;
		case 'T':
			settimeSSM( atof( optarg ) );
			if( gIsVerbose )
				printf( "set ssm time to %lf.\n", atof( optarg ) );
			break;
		case 'X':
			settimeSSM_speed( atof( optarg ) );
			if( gIsVerbose )
				printParam( 'x' );
			break;
		case 's':
			settimeSSM_speed( gettimeSSM_speed(  ) * 1.1 );
			if( gIsVerbose )
				printParam( 'x' );
			break;
		case 'S':
			settimeSSM_speed( gettimeSSM_speed(  ) / 1.1 );
			if( gIsVerbose )
				printParam( 'x' );
			break;
		case 'P':
			settimeSSM_is_pause( ( atoi( optarg ) ? 1 : 0 ) );
			if( gIsVerbose )
				printParam( 'p' );
			break;
		case 'R':
			settimeSSM_is_reverse( ( atoi( optarg ) ? 1 : 0 ) );
			if( gIsVerbose )
				printParam( 'r' );
			break;
		case 'g':
			settimeSSM_is_pause( ( gettimeSSM_is_pause(  )? 0 : 1 ) );
			if( gIsVerbose )
				printParam( 'p' );
			break;
		case 'G':
			settimeSSM_speed( -gettimeSSM_speed(  ) );
			printParam( 'r' );
			break;
		case 'v':
			gIsVerbose = 1;
			break;
		case 'q':
			gIsVerbose = 0;
			break;
		case 'h':
			printHelp( aArgv[0], 0 );
			return 1;
			break;
		case 'H':
			printHelp( aArgv[0], 1 );
			return 1;
			break;
		case 0:
			{
				switch ( optFlag )
				{
				case 's':
					sleepSSM( atof( optarg ) );
					break;
				case 'u':
					usleepSSM( atoi( optarg ) );
					break;
				default:
					break;
				}
			}
			break;
		default:
			{
				fprintf( stderr, "help : %s -h\n", aArgv[0] );
				return 1;
			}
			break;
		}
	}

	if( aArgc == 1 && init(  ) )
	{
		printParam( 'a' );
	}
	
	endSSM(  );

	return 0;
}
