/* 
 * libssm.c - SSM access APIs 2002-12-13 
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <time.h>
#include "libssm.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int shm_id = -1;								/**< Shared memory ID */
char *shm_ptr;									/**< Shared memory top address */
int msq_id = -1;								/**< Message queue ID */
pid_t my_pid;									/**< Own process ID */
char err_msg[20];

/**
 * @brief テーブルサイズの計算
 */
int calcSSM_table( ssmTimeT life, ssmTimeT cycle )
{
	return (( double )SSM_BUFFER_MARGIN * ( life / cycle ) ) + SSM_MARGIN;
}

/**
 * @brief テーブルサイズより保存時間の計算
 */
ssmTimeT calcSSM_life( int table_num, ssmTimeT cycle )
{
	return (ssmTimeT)( table_num - SSM_MARGIN ) * cycle / (ssmTimeT)SSM_BUFFER_MARGIN;
}


/*== TID functions =======================================================*/
/**
 * @brief 最新のTimeIDを返す
 * @return 最新のTID
 */
SSM_tid getTID_top( SSM_sid sid )
{
	if( sid == 0 )
		return SSM_ERROR_NO_DATA;
	/* return tid */
	return shm_get_tid_top( shm_get_address( sid ) );
}

/**
 * @brief 最旧のTimeIDを返す
 * @return 最旧のTID
 */
SSM_tid getTID_bottom( SSM_sid sid )
{
	if( sid == 0 )
		return SSM_ERROR_NO_DATA;
	return shm_get_tid_bottom( shm_get_address( sid ) );
}


/**
 * @brief 最新のTimeIDが指定した値以上になるのを待つ
 * @retval 1 成功
 * @retval 0 失敗
 * @retval -1 エラー
 */
int waitTID( SSM_sid sid, SSM_tid tid )
{
	if( sid == 0 )
		return -1;
	return shm_cond_wait( shm_get_address( sid ), tid );
}
 
/**
 * @brief 指定した時刻より前で、指定した時刻に最も近いTimeIDを返す(全探索)
 * @return 指定された時間に最も近いTID
 */
SSM_tid getTID( SSM_sid sid, ssmTimeT ytime )
{
	ssm_header *shm_p;
	SSM_tid r_tid;
	int top, bottom;

	if( sid == 0 )
		return SSM_ERROR_NO_DATA;

	/* get shared memory address of assigned sensor */
	shm_p = shm_get_address( sid );

	top = shm_get_tid_top( shm_p );
	bottom = shm_get_tid_bottom( shm_p );
	
	/* time check */
	if( ytime > shm_get_time( shm_p, top ) )
		return top;//SSM_ERROR_FUTURE; // -1
	if( ytime < shm_get_time( shm_p, bottom) )
		return SSM_ERROR_PAST; // -2

	/* search tid */
	for( r_tid = top; r_tid >= bottom; r_tid-- )
	{
		if( shm_get_time( shm_p, r_tid ) < ytime )
			break;
	}
	
	return r_tid;
}

/**
 * @brief search TimeID using stream cycle.
 * @return TID the nearest to time.
 */
SSM_tid getTID_time( ssm_header *shm_p, ssmTimeT ytime )
{
	//int table_pos;
	SSM_tid tid;
	//SSM_tid *tid_p;
	SSM_tid top, bottom;
	ssmTimeT top_time;
	//long long table_pos_long;

	top = shm_get_tid_top( shm_p );
	bottom = shm_get_tid_bottom( shm_p );
	
	/* time check */
	if( ytime > shm_get_time( shm_p, top ) )// topより大きいときは最も近いのはtopなので
		return top;//SSM_ERROR_FUTURE; // -1
	if( ytime < shm_get_time( shm_p, bottom) )
		return SSM_ERROR_PAST; // -2

	/* time quantize */
	/* センサ周期を使用して目標時刻の位置を予測してジャンプ */
	top_time = shm_get_time( shm_p, top );
	tid = top + ( SSM_tid )( ( ytime - top_time ) /  ( ssmTimeT )shm_p->cycle * gettimeSSM_speed(  ) );
	/* 予測がはずれていたときのための保険 */
	if( tid > top )
		tid = top;
	else if( tid < bottom )
		tid = bottom;

	/* tid search */
	while( shm_get_time( shm_p, tid ) < ytime )
		tid++;
	while( shm_get_time( shm_p, tid ) > ytime )
		tid--;

	return tid;
}


/*=======================MSG functions=========================================*/
/**
 * @brief Delete old message
 * 
 * ユーザーからは見える必要ないよね？
 * とりあえず消してみる。
 */
void del_msg( void )
{
	int len;
	ssm_msg msg;
	while( ( len = msgrcv( msq_id, &msg, SSM_MSG_SIZE, my_pid, IPC_NOWAIT ) ) > 0 );
}

/**
 * @brief coordinatorへの送信
 * @param[in,out] cmd_type コマンドの種類
 * @param[in] msg メッセージ
 * @retval 1 成功
 * @retval 0 失敗
 */
int send_msg( int cmd_type, ssm_msg *msg )
{
	ssm_msg msgbuf;
	/* initialize check */
	if( msq_id < 0 )
	{
		strcpy( err_msg, "msq id err" );
		return 0;
	}
	
	if( msg == NULL )
		msg = &msgbuf;
	
	msg->msg_type = MSQ_CMD;				
	msg->res_type = my_pid;					/* return pid */
	msg->cmd_type = cmd_type;
	
	/* send */
	if( msgsnd( msq_id, ( void * )( msg ), ( uint64_t ) SSM_MSG_SIZE, 0 ) < 0 )
	{
		perror( "msgsnd" );
		sprintf( err_msg, "msq send err" );
		return 0;
	}

	return 1;
}

/**
 * @brief coordinatorからの受信
 * @param[out] msg メッセージ
 * @retval 1 成功
 * @retval 0 失敗
 */
int receive_msg( ssm_msg *msg )
{
#ifdef __APPLE__
        volatile int64_t len;
#else
	int64_t len;
#endif
	/* initialize check */
	if( msq_id < 0 )
	{
		strcpy( err_msg, "msq id err" );
		return 0;
	}

	/* receive */
	len = msgrcv( msq_id, msg, SSM_MSG_SIZE, my_pid, 0 );
	if( len <= 0 )
	{
		strcpy( err_msg, "receive data err" );
		return 0;
	}

	return 1;
}


/**
 * @brief coordinatorとの通信
 * @param[in,out] msg メッセージ
 * @retval 1 成功
 * @retval 0 失敗
 */
int communicate_msg( int cmd_type, ssm_msg *msg )
{
	/* send */
	//printf("%d\n", cmd_type);
	send_msg( cmd_type, msg );
	/* ...processing on ssm side */
	
	/* return (msgrcv is waiting mode) */
	if( !receive_msg( msg ) )
		return 0;
	
	return 1;
}


/*=======================API functions=========================================*/
/* print err message */
void errSSM( void )
{
	fprintf( stderr, "SSM Err: %s\n", err_msg );
}
/* Initialize shared memory and message queue */
int initSSM( void )
{
	/* Open message queue */
	if( ( msq_id = msgget( ( key_t ) MSQ_KEY, 0666 ) ) < 0 )
	{
		sprintf( err_msg, "msq open err" );
		printf("%s", err_msg);
		return 0;
	}
	my_pid = getpid(  );

	/* 内部時刻同期変数への接続 */
	if( !opentimeSSM() )
	{
		sprintf( err_msg, "time open err" );
		printf("%s", err_msg);
		return 0;
	}
	
	/* SSMへの登録 */
	if( !send_msg( MC_INITIALIZE, NULL ) ) {
		printf("MC_INITIALIZE error\n");
		return 0;
	}
	
	return 1;
}

/**
 * @brief SSMの終了処理
 * @retval 1 成功
 * @retval 0 失敗
 */
int endSSM( void )
{
	int ret = 1;
	/* SSMへの登録 */
	if( !send_msg( MC_TERMINATE, NULL ) )
		ret = 0;
	
	/* 時間メモリの解除 */
	closetimeSSM(  );
	
	return ret;
}

/*------------Allocate sensor data space with time table on SSM  -----*/
SSM_sid createSSM( const char *name, int stream_id, uint64_t ssm_size, ssmTimeT life, ssmTimeT cycle )
{
	ssm_msg msg;
	int open_mode = SSM_READ | SSM_WRITE;
	ssm_header *shm_p;
	uint64_t len;
	/* initialize check */
	if( !name )
	{
		fprintf( stderr, "SSM ERROR : create : stream name is NOT defined, err.\n" );
		strcpy( err_msg, "name undefined" );
		return 0;
	}
	len = strlen( name );
	if( len == 0 || len >= SSM_SNAME_MAX )
	{
		fprintf( stderr, "SSM ERROR : create : stream name length of '%s' err.\n", name );
		strcpy( err_msg, "name length" );
		return 0;
	}
	
	if( stream_id < 0 )
	{
		fprintf( stderr, "SSM ERROR : create : stream id err.\n" );
		return 0;
	}
	
	if( life <= 0.0 )
	{
		fprintf( stderr, "SSM ERROR : create : stream life time err.\n" );
	}
	
	if( cycle <= 0.0 )
	{
		fprintf( stderr, "SSM ERROR : create : stream cycle err.\n" );
		strcpy( err_msg, "arg error : cycle" );
		return 0;
	}

	if( life < cycle )
	{
		fprintf( stderr, "SSM ERROR : create : stream saveTime MUST be larger than stream cycle.\n" );
		strcpy( err_msg, "arg err : life, c" );
		return 0;
	}


	/* prepare message packet */
	strncpy( msg.name, name, SSM_SNAME_MAX );
	msg.suid = stream_id;
	msg.ssize = ssm_size;
	msg.hsize = calcSSM_table( life, cycle ) ;	/* table size */
	msg.time = cycle;

	/* communicate */
	if( !communicate_msg( MC_CREATE | open_mode, &msg ) )
		return 0;

	/* return value: suid=sensors shm_id, ssize=offset */
	if( !msg.suid )
	{
		strcpy( err_msg, "shm address err" );
		return 0;
	}

	/* Attach to shared memory */
	if( ( shm_p = ( ssm_header * )shmat( msg.suid, 0, 0 ) ) == ( ssm_header * )-1 )
	{
		shm_p = 0;
		strcpy( err_msg, "shmat err" );
		return 0;
	}
	//sprintf( err_msg, "%d %p", msg.suid, shm_p );
	shm_p = shm_p + msg.ssize;				/* offset */
	

	shm_init_time( shm_p );
	return ( SSM_sid ) shm_p;
}

/**
 * @brief release stream
 *
 * ストリームを破棄したとき、このプロセスを使用している他の全てのプロセスが、
 * ストリームが破棄されたことを知る必要がある。
 * それができない間は、ただSSMから切断するだけの方が安全である。
 * 
 * とりあえずcreateとの対応で、作っておく。
 */
int releaseSSM( SSM_sid *sid )
{
	// TODO:破棄できるようにする
	// TODO:offset分も考慮する必要がある。
	// 現状offsetが未実装のため、考慮する必要が無い。
	if( *sid )
	{
		// TODO:要検討！shmdt
		//shmdt( *sid );
		*sid = 0;
	}
	return 1;
}

/*----------------------Open sensor data on SSM-----------------*/
SSM_sid openSSM( const char *name, int stream_id, char open_mode )
{
	ssm_msg msg;
	int len;
	ssm_header *shm_p;

	/* initialize check */
	len = strlen( name );
	if( len == 0 || len >= SSM_SNAME_MAX )
	{
		fprintf( stderr, "SSM ERROR : open : stream name length of '%s' err.\n", name );
		strcpy( err_msg, "name length" );
		return 0;
	}
	
	if( stream_id < 0 )
	{
		fprintf( stderr, "SSM ERROR : open : stream id err.\n" );
		return 0;
	}
	
	/* prepare message packet */
	strncpy( msg.name, name, SSM_SNAME_MAX );
	msg.suid = stream_id;
	msg.ssize = 0;
	msg.hsize = 0;
	msg.time = 0;

	/* communicate */
	if( !communicate_msg( MC_OPEN | ( int )open_mode, &msg ) )
		return 0;

	/* coordinatorからmsgが帰ってくる */

	/* attach to shared memory */
	if( msg.suid < 0 )
		return 0;

	if( ( shm_p = ( ssm_header * )shmat( msg.suid, 0, 0 ) ) == ( ssm_header * )-1 )
	{
		strcpy( err_msg, "shmat err" );
		shm_p = 0;
		return 0;
	}

	shm_p += msg.ssize;							/* offset */
	return ( SSM_sid ) shm_p;					/* sensor sid */
}

/** @brief close stream */
int closeSSM( SSM_sid *sid )
{
	// TODO:coordinatorに伝えないと意味が無い

	// TODO:offset分も考慮する必要がある。
	// 現状offsetが未実装のため、考慮する必要が無い。
	if( *sid )
	{
		// TODO:要検討！shmdt
		//shmdt( *sid );
		*sid = 0;
	}

	return 1;
}


/** @brief copy data to shared memory */
static void write_data( void *ssmp, const void *data, void *user_data )
{
	uint64_t size = *( uint64_t *)user_data;
//	printf("datasize... %llu\n", size);
        /*        
        char* p = (char*)data;

        printf("data(%p)\n", data);
        printf("-------> ");
        for (int i = 0; i < 8; ++i) {
            printf("%02x ", p[i] & 0xff);
        }
        printf("\n");
        */
        
        
	memcpy( ssmp, data, size );
}

/** @brief copy data to shared memory */
static void read_data( const void *ssmp, void *data, void *user_data )
{
	uint64_t size = *( uint64_t *)user_data;
	memcpy( data, ssmp, size );
}

/*------------------ read sensor data -----------------*/
SSM_tid readSSMP( SSM_sid sid, void *adata, ssmTimeT * ytime, SSM_tid tid,
	void (*callback)( const void *ssmp, void *data, void *user_data ), void *user_data )
{
	ssm_header *shm_p;
	SSM_tid top, bottom;
	char *data = ( char * )adata;

	if( sid == 0 )
		return SSM_ERROR_NO_DATA;

	shm_p = shm_get_address( sid );

	top = shm_get_tid_top( shm_p );
	bottom = shm_get_tid_bottom( shm_p );
	/* if tid is negative then the function returns latest data */
	if( tid < 0 )
		tid = top;

	/* region check */
	if( tid < SSM_TID_SP )
		return SSM_ERROR_NO_DATA;
	if( tid > top )
		return SSM_ERROR_FUTURE;
	if( tid < bottom )
		return SSM_ERROR_PAST;	
	//memcpy( data, , shm_p->size );
	callback( shm_get_data_ptr( shm_p, tid ), data, user_data );

	/* get time stamp */
	if( ytime )
	{
		// *ytime = ( ( ssmTimeT * ) ( ( char * )shm_p + shm_p->times_off ) )[r_tid];
		*ytime = shm_get_time( shm_p , tid );
	}
	// printf("%d ",r_tid);
	return tid;
}

SSM_tid readSSM( SSM_sid sid, void *adata, ssmTimeT *ytime, SSM_tid tid )
{
	return readSSMP( sid, adata, ytime, tid, read_data, &shm_get_address( sid )->size );
}


/*--------------------時間指定でデータを読み込む--------------------------
 * 
 * SIDで指定したセンサのデータを時刻指定で検索し読み込む
 * 
 * 　共有メモリのみのアクセスなので、プロセススイッチを必要としない。
 * 　が、TID指定より若干時間がかかる。
 * 
 * int readSSM_time(
 * SSM_sid sid, センサのID
 * void *data, データを返すハコ
 * ssmTimeT ytime, 指定時間
 * ssmTimeT ret_time 該当データの時間
 * )
 * 
 *-------------------------------------------------------------------*/
SSM_tid readSSMP_time( SSM_sid sid, void *adata, ssmTimeT ytime, ssmTimeT *ret_time ,
	void (*callback)( const void *ssmp, void *data, void *user_data ), void *user_data )
{
	SSM_tid tid;

	if( sid == 0 )
		return SSM_ERROR_NO_DATA;

	/* テーブルからTIDを検索 */
	if( ytime <= 0 )
	{											/* timeが負の時は最新データの読み込みとする */
		tid = -1;
	}
	else
	{
		tid = getTID_time( shm_get_address( sid ), ytime );
		// sprintf(err_msg,"tid %d",tid);
		if( tid < 0 )
			return tid;

	}
	return readSSMP( sid, adata, ret_time, tid, callback, &shm_get_address( sid )->size );
}

SSM_tid readSSM_time( SSM_sid sid, void *adata, ssmTimeT ytime, ssmTimeT *ret_time )
{
	return readSSMP_time( sid, adata, ytime, ret_time, read_data, &shm_get_address( sid )->size );
}

/*----------------------データを書き込む-----------------*/
SSM_tid writeSSMP( SSM_sid sid, const void *adata, ssmTimeT ytime ,
	void (*callback)( void *ssmp, const void *data, void *user_data ), void *user_data )
{
	ssm_header *shm_p;
	SSM_tid tid;
	//int i;
	const char *data = ( const char * )adata;

	if( sid == 0 )
		return SSM_ERROR_NO_DATA;

	shm_p = shm_get_address( sid );
	//i = 0;
	
	shm_lock( shm_p );
	{
		tid = shm_get_tid_top( shm_p ) + 1;

		//i = setTID_time( shm_p, ytime, tid );
		//memcpy( shm_get_data_ptr( shm_p, tid ), data, shm_p->size );
		/*
		printf("write addr = %p\n", shm_get_data_ptr( shm_p, tid ));
		char *p = (char*)data;
		for (int j = 0; j < 8; j++) {
			printf("%02x ", p[j] & 0xff);
		}
		printf("\n");

		p = shm_get_data_ptr( shm_p, tid );
		*/
		callback( shm_get_data_ptr( shm_p, tid ), data, user_data );
		/*
		for (int j = 0; j < 8; j++) {
			printf("%02x ", p[j] & 0xff);
		}
		printf("\n");
		*/
	
		/* 時刻の登録処理（y_timeが0の場合は現在の時刻） */
		shm_set_time( shm_p, tid, ytime );
	
		/* 更新 */
		( shm_p->tid_top )++;
	}
	shm_cond_broadcast( shm_p );
	shm_unlock( shm_p );

	return tid;
}

SSM_tid writeSSM( SSM_sid sid, const void *adata, ssmTimeT ytime )
{
	return writeSSMP( sid, adata, ytime, write_data, &shm_get_address( sid )->size );
}

SSM_tid writeSSM_time( SSM_sid sid, const void *adata, ssmTimeT ytime )
{
	return writeSSMP( sid, adata, ytime, write_data, &shm_get_address( sid )->size );
}

SSM_tid writeSSMP_time( SSM_sid sid, const void *adata, ssmTimeT ytime ,
	void (*callback)( void *ssmp, const void *data, void *user_data ), void *user_data )
{
	return writeSSMP( sid, adata, ytime, callback, user_data );
}

/*----------------------センサのプロパティのセット-----------------*/
int set_propertySSM( const char *name, int sensor_uid, const void *adata, uint64_t size )
{
	ssm_msg msg;
	char *ptr;
	const char *data = ( char * )adata;

	if( strlen( name ) > SSM_SNAME_MAX )
	{
		strcpy( err_msg, "name length" );
		return 0;
	}

	/* メッセージをセット */
	strncpy( msg.name, name, SSM_SNAME_MAX );
	msg.suid = sensor_uid;
	msg.ssize = size;
	msg.hsize = 0;
	msg.time = 0;

	/* communicate */
	if( !communicate_msg( MC_STREAM_PROPERTY_SET, &msg ) )
		return 0;

	if( !msg.ssize )
		return 0;								/* エラー */

	ptr = malloc( size + sizeof ( uint64_t ) );
	memcpy( ptr + sizeof ( uint64_t ), data, size );
	*( ( uint64_t * )ptr ) = msg.res_type;
	/* 送信 */
	msgsnd( msq_id, ptr, size, 0 );
	free( ptr );
	return 1;
}

/*----------------------センサのプロパティの取得-----------------*/
int get_propertySSM( const char *name, int sensor_uid, void *adata )
{
	ssm_msg msg;
	int size;
	char *ptr;
	char *data = ( char * )adata;

	if( strlen( name ) > SSM_SNAME_MAX )
	{
		strcpy( err_msg, "name length" );
		return 0;
	}

	/* メッセージをセット */
	strncpy( msg.name, name, SSM_SNAME_MAX );
	msg.suid = sensor_uid;
	msg.ssize = 0;
	msg.hsize = 0;
	msg.time = 0;

	/* communicate */
	if( !communicate_msg( MC_STREAM_PROPERTY_GET, &msg ) ) {
		return 0;
	}


	if( !msg.ssize ) {
		return 0;								/* エラー */
	}

	size = msg.ssize;

	ptr = malloc( size + sizeof ( uint64_t ) );

	/* 受信 */
	msgrcv( msq_id, ptr, size, my_pid, 0 );

	memcpy( data, ptr + sizeof ( uint64_t ), size );
	free( ptr );
	return 1;
}


//=============================================================================
/* モードに応じてデータを出力 */
int damp( SSM_sid sid, int mode, int num )
{
#if 0
	//int i;
	//SSM_tid *p;
	ssm_header *shm_p;

	/* 共有メモリのアドレスげっと */
	shm_p = shm_get_address( sid );

	switch ( mode )
	{
	case 0:

		break;
	case 1:

		break;
	case 2:									/* time table damp */
#if 0
		p = shm_get_timetable_address( shm_p );
		for ( i = 0; i < num; i++ )
		{
			printf( "table[%d] = %d \n ", i, (int)p[i] );
		}
#endif
		break;
	default:
		break;
	}
	return num;
#else
	return 0;
#endif
}

/**
 * @brief 現在登録されているSSM数を得る
 * @return センサの登録数
 * @retval エラーのとき負の値
 */
int getSSM_num( void )
{
	ssm_msg msg;

	/* communicate */
	if( !communicate_msg( MC_STREAM_LIST_NUM, &msg ) )
		return -1;

	return msg.suid;							/* 数を返す */
}

/* n番めのセンサの名前情報を読み込む */
int getSSM_name( int n, char *name, int *suid, uint64_t *size )
{
	ssm_msg msg;

	msg.suid = n;
	/* communicate */
	if( !communicate_msg( MC_STREAM_LIST_NAME, &msg ) )
		return -1;
		
	if( suid )
		*suid = msg.suid; /* ID */
	if( size )
		*size = msg.ssize;					/* データサイズ */
	if( name )
		strncpy( name, msg.name, SSM_SNAME_MAX );				/* 名前 */

	return msg.ssize;							/* サイズを返す(負なら存在しない) */
}

/* 指定した名前のセンサの情報を読み込む */
int getSSM_info( const char *name, int suid, uint64_t *size, int *num, double *cycle, uint64_t *property_size )
{
	ssm_msg msg;

	if( strlen( name ) > SSM_SNAME_MAX )
	{
		strcpy( err_msg, "name length" );
		return 0;
	}

	/* メッセージをセット */
	msg.suid = suid;
	strncpy( msg.name, name, SSM_SNAME_MAX );
	/* communicate */
	if( !communicate_msg( MC_STREAM_LIST_INFO, &msg ) )
		return -1;

	if( size )
		*size = msg.ssize;					/* データサイズ */
	if( num )
		*num = msg.hsize;						/* 履歴数 */
	if( cycle )
		*cycle = msg.time;					/* 周期 */
	if( property_size )
		*property_size = msg.suid;			/* 周期 */

	return msg.ssize;							/* サイズを返す(負なら存在しない) */
}


/* SSMに接続しているノード（プロセス）の個数を取得する */
int getSSM_node_num( void )
{
	ssm_msg msg;

	/* communicate */
	if( !communicate_msg( MC_NODE_LIST_NUM, &msg ) )
		return -1;

	return msg.suid;							/* 数を返す */
}

/* n番目のノードの情報を取得する */
int getSSM_node_info( int n, int *node_num )
{
	ssm_msg msg;

	msg.suid = n;
	if( !communicate_msg( MC_NODE_LIST_INFO, &msg ) )
		return 0;
	
	if( msg.suid < 0 )
		return 0;
	
	*node_num = msg.suid;
	
	return 1;
}

/* ノード同士を繋ぐエッジの個数を取得する */
int getSSM_edge_num( void )
{
	ssm_msg msg;

	/* communicate */
	if( !communicate_msg( MC_EDGE_LIST_NUM, &msg ) )
		return -1;

	return msg.suid;							/* 数を返す */
}

/* n番目のエッジの情報を取得する */
int getSSM_edge_info( int n, char *name, uint64_t name_size, int *id, int *node1, int *node2, int *dir )
{
	int64_t len;
	ssm_msg msg;
	ssm_msg_edge edge;

	msg.suid = n;
	if( !send_msg( MC_EDGE_LIST_INFO, &msg ) )
		return 0;
	
	/* receive */
	len = msgrcv( msq_id, &edge, sizeof(ssm_msg_edge) - sizeof(uint64_t), my_pid, 0 );
	if( len <= 0 )
	{
		strcpy( err_msg, "receive data err" );
		return 0;
	}

	if( !strlen( edge.name ) )
		return 0;	
	
	strncpy( name, edge.name, ( SSM_MSG_SIZE < name_size ? SSM_MSG_SIZE : name_size ) );
	*id = edge.suid;
	*node1 = edge.node1;
	*node2 = edge.node2;
	*dir = edge.cmd_type & SSM_MODE_MASK;
	
	return 1;
}
