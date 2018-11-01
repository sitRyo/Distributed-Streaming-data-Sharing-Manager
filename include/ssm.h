/**
 * @file ssm.h
 * @brief SSMをCで使うためのAPI
 */

#ifndef __SSM_H__
#define __SSM_H__



#include <stdint.h>
#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif

#include "ssm-time.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @addtogroup SSM_API */
/*@{*/

#define SSM_SNAME_MAX 32						/**< センサ名の最大長さ（8n） */
#define createSSM_time(n,u,s,l,c) createSSM(n,u,s,l,c)

	/**
	 * @brief オープンモードのフラグ
	 *
	 * まだフラグを設定しても、プログラムには影響を与えない
	 */
	typedef
	enum
	{
		SSM_READ = 0x20,							/**< 読み込みフラグ */
		SSM_WRITE = 0x40,							/**< 書き込みフラグ */
		SSM_EXCLUSIVE = 0x80,						/**< 排他フラグ(未実装) */
		SSM_MODE_MASK = 0xe0						/**< フラグを取り出すためのマスク */
	} SSM_open_mode;

	/**
	 * @brief TIDのエラー値
	 */
	enum
	{
		SSM_ERROR_FUTURE = -1,						/**< TIDエラー 新しすぎて存在しない */
		SSM_ERROR_PAST = -2,						/**< TIDエラー 古すぎて存在しない */
		SSM_ERROR_NO_DATA = -3						/**< TIDエラー SSM上にデータがまだ存在しない */
	};

	typedef char *SSM_sid;						/**< ストリーム記憶用の型 */
	typedef int SSM_tid;						/**< SSMのデータの書き込み回数カウンタ型 */

	/* ---- function prototypes ---- */

	/**
	 * @brief SSMでエラーが発生したときに詳細を表示する
	 */
	void errSSM( void );

	/* system */
	/**
	 * @brief SSMへの接続
	 * @retval 1 成功
	 * @retval 0 失敗
	 *
	 * すべてのSSMの関数を使用する前にかならず実行する。
	 * これを実行しない場合、他の関数は失敗する。
	 */
	int initSSM( void );

	/**
	 * @brief SSMからの切断
	 * @retval 1 成功
	 * @retval 0 失敗
	 *
	 * SSMの使用をやめる場合に必ず実行すること。
	 */
	int endSSM( void );
	
	/* open */
	/**
	 * @brief ストリームの作成
	 * @param[in] stream_name ストリームの名前
	 * @param[in] stream_id 同一ストリーム名を区別するための番号
	 * @param[in] stream_size ストリームのデータサイズ
	 * @param[in] life ストリームの保存時間
	 * @param[in] cycle ストリームの書き込み周期
	 * @return SSM管理ID
	 * @retval 0 失敗
	 *
	 * SSMはストリームの管理を"名前"と"番号"を使用して管理する。
	 * 同一の機能を持つストリームは、名前を同じにし、番号を変えて管理すると分かりやすい。
	 * 
	 * ストリームのデータサイズは、書き込む構造体（クラス）などのサイズを入れる。
	 *
	 * SSMは時刻同期用にリングバッファを構成する。
	 * リングバッファの個数は、life及びcycleに従って確保される。
	 * また、cycleは高速な時刻同期にも利用されるため、できるだけ正確に書くことが望ましい。
	 *
	 * 終了時には destroySSM() しなければならない。
	 *
	 * @see destroySSM, openSSM
	 */
	SSM_sid createSSM( const char *stream_name, int stream_id, uint64_t stream_size, ssmTimeT life, ssmTimeT cycle ) __attribute__ ((warn_unused_result));
	
	/**
	 * @brief ストリームの破棄
	 * @param[in,out] sid SSM管理ID
	 * @retval 1 成功
	 * @retval 0 失敗
	 *
	 * 現状として、実際にSSMからストリームを破棄できるわけではなく、ストリームから切断するだけである。
	 * しかし、これからのことを考えると、プログラムの最後で必ず実行しておくことを推奨する。
	 */
	int releaseSSM( SSM_sid *sid );
	
	/**
	 * @brief ストリームへの接続
	 * @param[in] stream_name ストリームの名前
	 * @param[in] stream_id 同一ストリーム名を区別するための番号
	 * @param[in] open_mode ストリームの書き込み方向を定義できるようにしたい。とりあえず０を書いておけば問題ない。
	 * @return SSM管理ID
	 * @retval 0 失敗
	 *
	 * すでに作成されたストリームへ接続する。
	 *
	 * openSSM() した場合は、終了時に closeSSM() を実行する必要がある
	 * @see closeSSM, createSSM
	 */
	SSM_sid openSSM( const char *stream_name, int stream_id, char open_mode ) __attribute__ ((warn_unused_result));

	/**
	 * @brief ストリームの破棄
	 * @param[in,out] sid SSM管理ID
	 * @retval 1 成功
	 * @retval 0 失敗
	 *
 	 * これからのアップデートを考えたとき、プログラム終了時にこの関数を呼び出すことが望ましい。
 	 * @see openSSM
	 */
	int closeSSM( SSM_sid *sid );

	/* tid */
	/**
	 * @brief TimeIDの探索
	 * @param[in] sid SSM管理ID
	 * @param[in]　ytime 目標時間
	 * @return 指定時刻より前のうち、最も近い時刻に書き込まれたデータのTID >= 0 を返す。
	 * @retval SSM_ERROR_FUTURE 新しすぎて存在しない
	 * @retval SSM_ERROR_PAST 古すぎて存在しない
	 * @retval SSM_ERROR_NO_DATA SSM上にデータがまだ存在しない
	 */
	SSM_tid getTID( SSM_sid sid, ssmTimeT ytime );
	
	/**
	 * @brief SSM上に存在する最も新しいTimeIDを取得する
	 * @param[in] sid SSM管理ID
	 * @return 最も新しいTimeID
	 */
	SSM_tid getTID_top( SSM_sid sid );
	
	/**
	 * @brief SSM上に存在する最も古いTimeIDを取得する
	 * @param[in] sid SSM管理ID
	 * @return 最も古いTimeID
	 */
	SSM_tid getTID_bottom( SSM_sid sid );
	
	/**
	 * @brief 指定したTimeIDになるまで待つ
	 * @param[in] sid SSM管理ID
	 * @param[in] tid 読み込みたいTimeID
	 * @retval 1 成功
	 * @retval 0 失敗
	 * @retval -1 エラー
	 * @warning 実装・戻り値などを変更する可能性があるので注意！
	 *
	 * 現状の実装として、waitできるのは１秒のみである。
	 */
	int waitTID( SSM_sid sid, SSM_tid tid );

	/* read */
	/**
	 * @brief 指定したTimeIDのデータを読み込む
	 * @param[in] sid SSM管理ID
	 * @param[out] *data 読み込むデータ
	 * @param[out] *ytime データの書き込み時間
	 * @param[in] tid 読み込みたいTimeID
	 *            -1を指定すると最新のデータ
	 * @return 読み込んだデータのTID >= 0 を返す。
	 * @retval SSM_ERROR_FUTURE 新しすぎて存在しない
	 * @retval SSM_ERROR_PAST 古すぎて存在しない
	 * @retval SSM_ERROR_NO_DATA SSM上にデータがまだ存在しない
	 */
	SSM_tid readSSM( SSM_sid sid, void *data, ssmTimeT * ytime, SSM_tid tid ) __attribute__ ((warn_unused_result));

	/**
	 * @brief 指定した時刻より前で、指定した時刻に最も近いデータを読み込む
	 * @param[in] sid SSM管理ID
	 * @param[out] *data 読み込むデータ
	 * @param[out] ytime 指定時刻
	 * @return 読み込んだデータのTID >= 0 を返す。
	 * @retval SSM_ERROR_FUTURE 新しすぎて存在しない
	 * @retval SSM_ERROR_PAST 古すぎて存在しない
	 * @retval SSM_ERROR_NO_DATA SSM上にデータがまだ存在しない
	 */
	SSM_tid readSSM_time( SSM_sid sid, void *data, ssmTimeT ytime, ssmTimeT * ret_time ) __attribute__ ((warn_unused_result));
	
	/* read with callback function */
	SSM_tid readSSMP( SSM_sid sid, void *adata, ssmTimeT * ytime, SSM_tid tid,
		void (*callback)( const void *ssmp, void *data, void *user_data ), void *user_data ) __attribute__ ((warn_unused_result));
	SSM_tid readSSMP_time( SSM_sid sid, void *adata, ssmTimeT ytime, ssmTimeT *ret_time ,
		void (*callback)( const void *ssmp, void *data, void *user_data ), void *user_data ) __attribute__ ((warn_unused_result));

	/* write */
	/**
	 * @brief 指定したタイムスタンプを付けてデータを書き込む
	 * @param[in] sid SSM管理ID
	 * @param[in] *data 書き込むデータ
	 * @param[in] ytime データの書き込み時間
	 * @return 書き込んだときのTimeIDを返す
	 */
	SSM_tid writeSSM_time( SSM_sid sid, const void *data, ssmTimeT ytime );

	/**
	 * @brief writeSSM_time と一緒。互換性のため、残してある。
	 * @see writeSSM_time
	 */
	SSM_tid writeSSM( SSM_sid sid, const void *data, ssmTimeT ytime );
	
	/* write with callback function */
	SSM_tid writeSSMP( SSM_sid sid, const void *adata, ssmTimeT ytime ,
		 void (*callback)( void *ssmp, const void *data, void *user_data ), void *user_data );
	SSM_tid writeSSMP_time( SSM_sid sid, const void *adata, ssmTimeT ytime ,
		void (*callback)( void *ssmp, const void *data, void *user_data ), void *user_data );

	/* property */
	/**
	 * @brief SSMへのプロパティーの登録
	 * @param[in] *stream_name ストリームの名前
	 * @param[in] stream_id 同一ストリーム名を区別するための番号
	 * @param[in] *data 書き込むデータ
	 * @param[in] size データサイズ
	 * @retval 1 成功
	 * @retval 0 失敗
	 *
	 * create済みのストリームしか登録できないので注意。
	 */
	int set_propertySSM( const char *stream_name, int stream_id, const void *data, uint64_t size );

	/**
	 * @brief SSMからのプロパティーの取得
	 * @param[in] *stream_name ストリームの名前
	 * @param[in] stream_id 同一ストリーム名を区別するための番号
	 * @param[out] *data 書き込むデータ
	 * @retval 1 成功
	 * @retval 0 失敗
	 */
	int get_propertySSM( const char *stream_name, int stream_id, void *data );

	/* others */
	int getSSM_num( void );
	int getSSM_name( int n, char *stream_name, int *stream_id, uint64_t *size );
	int getSSM_info( const char *stream_name, int stream_id, uint64_t *size, int *num, double *cycle, uint64_t *property_size );
	
	/**
	 * @brief メモリダンプ
	 * @warning 廃止予定 
	 */
	int damp( SSM_sid sid, int mode, int num );

/*@}*/

#ifdef __cplusplus
}
#endif

#endif
