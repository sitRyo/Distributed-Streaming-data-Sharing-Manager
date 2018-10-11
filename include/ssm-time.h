/** 
 * @file ssm-time.h
 * @brief SSM 用共通時間ライブラリ用ヘッダ
 * 
 * 特に特別なことをするわけではなく、時間に共通性を持たせるため。
 * 
 * ssmTimeT は現状では double 型でやってるけど、本来は long 型でやる方が最小分解能の関係上適している。
 * でもなんかめんどいのでとりあえず double 型。
 * 
 * 2003-01-22 やはり long にする。 2003-01-22 ・・・やっぱdouble。 
 */

#ifndef __SSM_TIME_H__
#define __SSM_TIME_H__

#include <unistd.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif
/** @addtogroup SSM_Time */
/*@{*/


	typedef double ssmTimeT;					/**< (sec)SSMの時間を表す型 */

	/** get ssm time */
	/**
	 * @brief SSM時刻を取得する
	 * @return SSM時刻
	 */
	ssmTimeT gettimeSSM( void );

	/**
	 * @brief 現在時刻を取得する
	 * @return 現在時刻
	 */
	ssmTimeT gettimeSSM_real( void );

	/** sleep with ssm */
	/**
	 * @brief SSMの再生速度に合わせたsleep
	 * @param[in] sec スリープする時間(sec)
	 * @retval 0 成功
	 * @retval -1 シグナルハンドラに割り込まれた
	 */
	unsigned int sleepSSM( double sec );
	
	/**
	 * @brief SSMの再生速度に合わせたusleep
	 * @param[in] usec スリープする時間(usec)
	 * @retval 0 成功
	 * @retval -1 シグナルハンドラに割り込まれた
	 */
	int usleepSSM( useconds_t usec );
#if _POSIX_C_SOURCE >= 199309L
	/**
	 * @brief SSMの再生速度に合わせたnanosleep
	 * @param[in] *req スリープする時間
	 * @param[in] *rem スリープが失敗したときの残り時間
	 * @retval 0 成功
	 * @retval -1 シグナルハンドラに割り込まれた
	 */
	int nanosleepSSM( const struct timespec *req, struct timespec *rem );
#endif

/*@}*/
#ifdef __cplusplus
}
#endif

#endif											/* __SSM_TIME_H__ */
