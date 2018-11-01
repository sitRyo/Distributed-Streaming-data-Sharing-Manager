#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

#include "libssm.h"

/*== Shared memory control functions =====================================*/
/* allock shm memory */
int shm_create_ssm( key_t key, int data_size, int history_num, double cycle )
{
	int shm_id;
	/* 共有メモリ領域をげと */
	/* ssmヘッダの後ろにデータとタイムスタンプのをくっつけるので、その分もまとめて確保している */
	shm_id = shmget( key, sizeof( ssm_header ) + ( data_size + sizeof ( ssmTimeT ) ) * history_num, IPC_CREAT | 0666 );
	if( shm_id < 0 )
	{
		return -1;
	}
	return shm_id;
}

/* open shm_memory */
ssm_header *shm_open_ssm( int shm_id )
{
	ssm_header *header;
	/* あたっち */
	header = shmat( shm_id, 0, 0 );
	if( header == ( ssm_header * )-1 )
	{
		return 0;
	}
	return header;
}

/* init shm header */
void shm_init_header( ssm_header *header, int data_size, int history_num, ssmTimeT cycle )
{
	pthread_mutexattr_t mattr;
	pthread_condattr_t cattr; 

	header->tid_top = SSM_TID_SP - 1;	/* 初期位置 */
	header->size = data_size;	/* データサイズ */
	header->num = history_num;	/* 履歴数 */
	//header->table_size = hsize;	/* テーブルサイズ */
	header->cycle = cycle;	/* データ最小サイクル */
	header->data_off = sizeof( ssm_header );	/* データの先頭アドレス */
	header->times_off = header->data_off + ( data_size * history_num );	/* 時刻の先頭アドレス */
	//header->table_off = header->times_off + sizeof( ssmTimeT ) * hsize ;	/* time tableの先頭アドレス */
	
	/* 同期用mutexの初期化 */
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&header->mutex, &mattr); 

	pthread_condattr_init(&cattr);
	pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(&header->cond, &cattr);
	
	pthread_mutexattr_destroy( &mattr );
	pthread_condattr_destroy( &cattr );
}

void shm_dest_header( ssm_header *header )
{
	pthread_mutex_destroy( &header->mutex );
	pthread_cond_destroy( &header->cond );
}


/* shared memory address */
ssm_header *shm_get_address( SSM_sid sid )
{
	return ( ssm_header * )sid;
}

/* data */
void *shm_get_data_address( ssm_header *shm_p )
{
	return ( void * )( (char *)shm_p + shm_p->data_off );
}

void *shm_get_data_ptr( ssm_header *shm_p, SSM_tid tid )
{
 	return ( void * )( (char *)shm_p + shm_p->data_off + (shm_p->size * ( tid % shm_p->num )));
}

uint64_t shm_get_data_size( ssm_header *shm_p )
{
	return shm_p->size;
}

/* ssmtime */
ssmTimeT *shm_get_time_address( ssm_header *shm_p )
{
	return ( ssmTimeT * ) ( (char *)shm_p + shm_p->times_off );
}

void shm_init_time( ssm_header *shm_p )
{
	int i;
	ssmTimeT *time = shm_get_time_address( shm_p );
	for(i = 0; i < shm_p->num; i++)
		time[i] = 0;
}

ssmTimeT shm_get_time( ssm_header *shm_p, SSM_tid tid )
{
	return shm_get_time_address( shm_p )[tid % shm_p->num];
}

void shm_set_time( ssm_header *shm_p, SSM_tid tid, ssmTimeT time )
{
	shm_get_time_address( shm_p )[tid % shm_p->num] = time;
}


/* tid */
#if 0
SSM_tid *shm_get_timetable_address( ssm_header *shm_p )
{
	return ( SSM_tid *)((char *)shm_p + shm_p->table_off);
}

void shm_init_timetable( ssm_header *shm_p )
{
	int i;
	SSM_tid *timetable = shm_get_timetable_address( shm_p );
	for ( i = 0; i < shm_p->table_size; i++ )
		timetable[i] = 0;
	timetable[0] = -1000000;

}
#endif
SSM_tid shm_get_tid_top( ssm_header *shm_p )
{
	/* return tid */
	return shm_p->tid_top;
}

SSM_tid shm_get_tid_bottom( ssm_header *shm_p )
{
	int tid;
	if( shm_p->tid_top < 0 )
		return shm_p->tid_top;
	/* return tid */
	tid = shm_p->tid_top - shm_p->num + SSM_MARGIN + 1;
	
	if( tid < 0 )
		return 0;
	
	return tid;
}

/**
 * @brief lock shm
 * @retval 1:success, 0: failed
 */
int shm_lock( ssm_header *shm_p )
{
	if( pthread_mutex_lock( &shm_p->mutex ) )
		return 0;
	return 1;
}

/**
 * @brief unlock shm
 * @retval 1:success, 0: failed
 */
int shm_unlock( ssm_header *shm_p )
{
	if( pthread_mutex_unlock( &shm_p->mutex ) )
		return 0;
	return 1;
}

/**
 * @brief 書き込まれたTIDが指定したtid以上になるまで待つ
 * @retval 1 成功
 * @retval 0 失敗
 * @retval -1 エラー
 */
int shm_cond_wait( ssm_header *shm_p, SSM_tid tid )
{
	int ret = 0;
	struct timeval now;
	struct timespec tout;
	
	if( tid <= shm_get_tid_top( shm_p ) )
		return 1;
	gettimeofday( &now, NULL );
	tout.tv_sec = now.tv_sec + 1;
	tout.tv_nsec = now.tv_usec * 1000;


	if( !shm_lock( shm_p ) )
		return -1;
	while( tid > shm_get_tid_top( shm_p ) && (ret == 0) )
	{
		ret = pthread_cond_timedwait( &shm_p->cond, &shm_p->mutex, &tout );
	}
	if( !shm_unlock( shm_p ) )
		return -1;
	return ( ret == 0 );
}

int shm_cond_broadcast( ssm_header *shm_p )
{
	pthread_cond_broadcast( &shm_p->cond );
	return 1;
}
