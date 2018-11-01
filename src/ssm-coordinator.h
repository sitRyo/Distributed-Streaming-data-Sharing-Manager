#ifndef __SSM_COORDINATOR_H__
#define __SSM_COORDINATOR_H__

#include "ssm.h"
#include "ssm-time.h"

/* ---- typedefs ---- */
typedef struct ssm_list *SSM_ListPtr;
typedef struct ssm_list
{
	int node;									/**< ストリームを作成したノード */
	char name[SSM_SNAME_MAX];					/**< ストリームの名前 */
	int suid;									/**< ストリーム固有のID */
	int shm_id;									/**< ブロックのある共有メモリのID */
	char *shm_ptr;								/**< ブロックのある共有メモリのアドレス(Manager内のみ有効) */
	int shm_offset;								/**< 共有メモリ内でブロックのある場所 */
	int size;									/**< ブロックのサイズ */
	ssm_header *header;							/**< ブロックのヘッダ */
	SSM_ListPtr next;							/**< 次のリストへ */
	char *property;								/**< プロパティデータへのポインタ */
	int property_size;							/**< プロパティデータの大きさ */
} SSM_List;

/* ---- function prototypes ---- */
int alloc_ssm_block( int ssize, int hsize, ssmTimeT cycle, char **shm_h, int *offset );
SSM_List *add_ssm_list( char *name, int suid, int ssize, int hsize, ssmTimeT cycle );
void free_ssm_list( SSM_List * ssmp );
int ssm_init( void );
SSM_List *serch_SSM_List( char *name, int suid );
int msq_loop( void );
SSM_List *get_nth_SSM_List( int n );
int get_num_SSM_List( void );
uint64_t get_receive_id( void );

#endif
