#include <stdio.h>
#include <string.h>
#include "ssm.h"
#include "ssm-time.h"

int get_unit( double *data, char *unit, double base )
{
	int i = 0;
	static const char table[] = { " kMGTPEZY" };
	while( *data >= base && i < 8 )
	{
		*data /= base;
		i++;
	}
	if( i < 0 || i > 8 )
		i = 0;
	*unit = table[i];

	if( i > 0 )
		return 1;
	return 0;
}

int main( int argc, char *argv[] )
{
	int sid_search;
	char name[100], size_unit, property_unit;
	int suid, num;
	uint64_t size, property_size;
	double cycle, size_human, property_size_human;

	if( !initSSM(  ) )
		return 0;
	// scanf("%d",&i);
	sid_search = 0;

	printf( "No. |  sensor name   | suid | size[B] |  num  | cycle[s] | property[B] |\n" );
	printf( "----+----------------+------+---------+-------+----------+-------------|\n" );
	while( getSSM_name( sid_search, name, &suid, &size ) >= 0 )
	{
		if( getSSM_info( name, suid, &size, &num, &cycle, &property_size ) < 0 )
		{
			printf( "ERROR: SSM read error.\n" );
			return -1;
		}
		size_human = size;
		property_size_human = property_size;
		get_unit( &size_human, &size_unit, 1024 );
		get_unit( &property_size_human, &property_unit, 1024 );

		printf( "%03d | %14.14s | %4d | %6.2f%c | %5d | %8.3f | %10.2f%c\n", sid_search, name, suid, size_human, size_unit, num, cycle, property_size_human, property_unit );
		sid_search++;
	}
	printf( "----+----------------+------+---------+-------+----------+-------------|\n" );

	printf( "Total %d sensors.\n", sid_search );

	endSSM(  );
	return 0;
}
