#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/times.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/termios.h>
#include <pthread.h>
#include <signal.h>


#include "ssm.h"
#include "ssm-time.h"
#include "ssm-transporter.h"

int sockfd, infd;
struct sockaddr_in srv;
#define SERVER_MODE 0
#define CLIENT_MODE 1

#define DRIVER_WAIT_TIMEOUT 600

char mode;
pthread_mutex_t mutex;


int gShutOff = 0;

void ctrlC( int aStatus )
{
	// exit(aStatus); // デストラクタが呼ばれないのでダメ
	gShutOff = 1;
	signal( SIGINT, NULL );
}

void setSigInt(  )
{
	signal(SIGINT, ctrlC);
}


/* サーバ側オープン */
int open_tcpip( int port_num )
{
	socklen_t socklen;

	if( ( sockfd = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		printf( "ERROR: socket error.\n" );
		return 0;
	}
	memset( &srv, 0, sizeof ( srv ) );
	srv.sin_family = AF_INET;
	srv.sin_port = htons( port_num );

	socklen = sizeof ( srv );
	if( ( bind( sockfd, ( struct sockaddr * )&srv, socklen ) ) < 0 )
	{
		printf( "binding..." );
		return 0;
	}
	if( listen( sockfd, 5 ) < 0 )
	{
		printf( "listening..." );
		return 0;
	}
	/* connect */
	puts( "TCP/IP socket available" );
	printf( "\tport %d\n", ntohs( srv.sin_port ) );
	printf( "\taddr %s\n", inet_ntoa( srv.sin_addr ) );
	if( ( infd = accept( sockfd, ( struct sockaddr * )&srv, &socklen ) ) >= 0 )
		puts( "new connection granted." );
	return 1;
}

int open_tcpip_cli( char *ip_address, int port_num )
{
	socklen_t socklen;
	struct sockaddr_in cli;

	if( ( infd = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 )
	{
		printf( "socket\n" );
		return 0;
	}
	memset( &cli, 0, sizeof ( cli ) );
	cli.sin_family = AF_INET;
	cli.sin_port = htons( port_num );
	if( !( inet_aton( ip_address, &cli.sin_addr ) ) )
	{
		printf( "inet_aton\n" );
		return 0;
	}
	socklen = sizeof ( cli );
	if( connect( infd, ( struct sockaddr * )&cli, socklen ) )
	{
		printf( "ERROR: connect error.\n" );
		return 0;
	}
	/* connect */
	puts( "connected to TCP/IP socket" );
	printf( "\tport %d\n", ntohs( cli.sin_port ) );
	printf( "\taddr %s\n", inet_ntoa( cli.sin_addr ) );
	return 1;
}

/* 新しいセンサを登録する */
int regist_new_sensor( SSMT_newsensor * new_sensor, RegSensPtr sensor, int num )
{
	sprintf( sensor->name, "%s", new_sensor->name );
	if( ( sensor->sid = createSSM_time( new_sensor->name, new_sensor->suid, new_sensor->size, new_sensor->time, new_sensor->cycle ) ) <= 0 )
	{
		printf( "ERROR: Sensor %s[%d] registration failed\n", new_sensor->name, new_sensor->suid );
		return 0;
	}
	sensor->size = new_sensor->size;
	sensor->suid = new_sensor->suid;
	sensor->property_size = new_sensor->property_size;
	sensor->data = malloc( new_sensor->size );
	if( new_sensor->property_size )
		sensor->property = malloc( new_sensor->property_size );
	else
		sensor->property = 0;
	sensor->id = num;
	if( !sensor->data )
	{
		printf( "ERROR: Sensor %s[%d] having no data\n", new_sensor->name, new_sensor->suid );
		return 0;
	}
	return 1;
}

/* TCP/IPで送信 */
int send_sensor_data( RegSensPtr sensor )
{
	int rp, size;
	SSMT_header header;

	/* ヘッダ送信 */
	header.head = SSMT_HEAD;
	header.type = SSMT_DATA;
	header.id = sensor->id;
	header.size = sensor->size;
	header.time = sensor->time;
	write( infd, &header, sizeof ( SSMT_header ) );

	/* データ送信 */
	rp = 0;
	while( rp < sensor->size )
	{
		size = 1000;
		if( size > sensor->size - rp )
			size = sensor->size - rp;
		rp += write( infd, sensor->data + rp, size );
		// printf(">");
	}
	return 1;
}

int send_newsensor( RegSensPtr sensor, char *name, int suid )
{
	static int id;
	SSMT_newsensor new_sensor;
	SSMT_header header;
	int num;

	int timeoutCnt = 0;

	printf( "INFO: Sending sensor %s[%d]...\n", name, suid );

	sprintf( new_sensor.name, "%s", name );
	new_sensor.suid = suid;

	if( ( sensor->sid = openSSM( name, suid, 0 ) ) <= 0 )
	{
		printf( "WARNING: Sensor %s[%d] is not found, wating for it!\n", name, suid );
		while( ( ( sensor->sid = openSSM( name, suid, 0 ) ) <= 0 ) && ( timeoutCnt <= DRIVER_WAIT_TIMEOUT ) )
		{
			usleep( 1000 );
			timeoutCnt += 0.001;
		}
		if( timeoutCnt >= DRIVER_WAIT_TIMEOUT )
		{
			printf( "ERROR: Sensor %s[%d] is not found and timeout reached\n", name, suid );
			return 0;
		}

		// return 0;
		usleep( 1000000 );
	}
	if( getSSM_info( name, suid, &new_sensor.size, &num, &new_sensor.cycle, &new_sensor.property_size ) < 0 )
		return 0;
	new_sensor.time = ( double )num *new_sensor.cycle;

	/* ヘッダ送信 */
	header.head = SSMT_HEAD;
	header.type = SSMT_NEW;
	header.id = 0;
	header.size = sizeof ( SSMT_newsensor );
	write( infd, &header, sizeof ( SSMT_header ) );
	write( infd, &new_sensor, sizeof ( SSMT_newsensor ) );
	/* プロパティ送信 */
	if( new_sensor.property_size > 0 )
	{
		sensor->property = malloc( new_sensor.property_size );
		printf( "get property\n" );
		get_propertySSM( name, suid, ( char * )sensor->property );
		header.head = SSMT_HEAD;
		header.type = SSMT_PROPERTY;
		header.id = id;
		header.size = new_sensor.property_size;
		write( infd, &header, sizeof ( SSMT_header ) );
		write( infd, ( char * )sensor->property, new_sensor.property_size );
	}
	else
	{
		sensor->property = 0;
	}
	sensor->id = id;
	sensor->suid = new_sensor.suid;
	sensor->size = new_sensor.size;
	sensor->property_size = new_sensor.property_size;
	sprintf( sensor->name, "%s", name );
	sensor->data = malloc( sensor->size );
	sensor->tid = readSSM( sensor->sid, sensor->data, &sensor->time, -1 );

	id++;
	return 1;
}

/* 新しいデータがあったら送信する */
int ssm2tcp( char *filename )
{
	RegisterdSensor out_sensor[20];
	int out_sensor_num = 0;
	int updated;
	char name[30];
	int suid, i;
	FILE *send_file;

	/* センサの登録処理 */
	send_file = fopen( filename, "r" );
	if( !send_file )
	{
		printf( "File [%s] does not exist. No driver data will be sent\n", filename );
		// return 0;
	}
	else
	{
		printf( "File [%s] does exist.\n", filename );
		while( fscanf( send_file, "%s %d", name, &suid ) != EOF )
		{
			if( send_newsensor( &out_sensor[out_sensor_num], name, suid ) )
			{
				printf( "Sensor %s[%d] sent.\n", out_sensor[out_sensor_num].name, out_sensor[out_sensor_num].suid );
				out_sensor_num++;
			}
		}
		fclose( send_file );
	}
	/* センサデータの送信処理 */
	while( !gShutOff )
	{
		updated = 0;
		for ( i = 0; i < out_sensor_num; i++ )
		{
			if( readSSM( out_sensor[i].sid, out_sensor[i].data, &out_sensor[i].time, out_sensor[i].tid ) > 0 )
			{
				out_sensor[i].tid++;
				send_sensor_data( &out_sensor[i] );	/* 送信する */
				updated = 1;
			}
		}
		if( !updated )
			usleep( 10000 );
	}
}

/* TCP/IPのメッセージを解釈して、SSMに入力する */
void tcp2ssm( void )
{
	int len;
	RegisterdSensor in_sensor[20];
	int in_sensor_num = 0;
	SSMT_header header;
	SSMT_newsensor new_sensor;

	while( !gShutOff )
	{
		/* ヘッダ読み込み */
		len = 0;
		while( len < sizeof ( SSMT_header ) )
			len += read( infd, ( ( char * )&header ) + len, sizeof ( SSMT_header ) - len );
		/* header check */
		if( header.head != SSMT_HEAD )
		{
			printf( "ERROR: header error.\n" );
			return;
		}

		/* それぞれのタイプに応じて読み込み */
		switch ( header.type )
		{
		case SSMT_NEW:
			len = 0;
			while( len < sizeof ( SSMT_newsensor ) )
				len += read( infd, ( ( char * )&new_sensor ) + len, sizeof ( SSMT_newsensor ) - len );
			if( !regist_new_sensor( &new_sensor, &in_sensor[in_sensor_num], in_sensor_num ) )
				return;
			printf( "sensor %s[%d] recieved and registerd.\n", in_sensor[in_sensor_num].name, in_sensor[in_sensor_num].suid );
			in_sensor_num++;
			break;
		case SSMT_PROPERTY:
			len = 0;
			while( len < header.size )
				len += read( infd, ( ( char * )in_sensor[header.id].property ) + len, header.size - len );
			set_propertySSM( in_sensor[header.id].name, in_sensor[header.id].suid, in_sensor[header.id].property, in_sensor[header.id].property_size );
			printf( "set property\n" );
			break;
		case SSMT_DATA:
			len = 0;
			while( len < header.size )
				len += read( infd, ( ( char * )in_sensor[header.id].data ) + len, header.size - len );
			writeSSM( in_sensor[header.id].sid, in_sensor[header.id].data, header.time );
			// printf("one data\n");
			break;
		default:
			break;
		}
	}
}

int main( int argc, char *argv[] )
{
	pthread_t receive_thread;

	/* initialize */
	pthread_mutex_init( &mutex, NULL );
	
	if( !initSSM(  ) )
		fprintf( stderr, "ERROR : cannot open ssm.\n" );
	
	setSigInt();
	
	if( argc > 1 )
	{
		mode = CLIENT_MODE;
		printf( "client\n" );
		open_tcpip_cli( argv[1], 50000 );
	}
	else
	{
		mode = SERVER_MODE;
		printf( "server\n" );
		open_tcpip( 50000 );
	}
	if( pthread_create( &receive_thread, NULL, ( void * )tcp2ssm, NULL ) != 0 )
	{
		/* エラー処理 */
	}

	if( mode == CLIENT_MODE )
		ssm2tcp( "send_sensors.client" );

	if( mode == SERVER_MODE )
		ssm2tcp( "send_sensors.server" );

	pthread_join( receive_thread, NULL );

	endSSM(  );

	return 0;
}
