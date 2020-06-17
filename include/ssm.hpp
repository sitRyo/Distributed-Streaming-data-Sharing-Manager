/**
 * @file ssm.hpp
 * @brief SSMを簡単に扱うためのクラス
 *
 *  @date 2009/11/08
 *  @author STK
 */

#ifndef __SSM_HPP__
#define __SSM_HPP__

#include <iostream>
#include "ssm.h"

#include "libssm.h"

/**
 * Dummy Class
 */
class SSMDummy
{
};


/**
 * SSMを簡単に扱うためのクラス
 *
 * これ単体では使用しづらいので、継承して使う
 */
class SSMApiBase
{
protected:
	const char *streamName;					///< ストリームの名前
	int streamId;								///< 同一ストリーム名を区別するための番号
	SSM_sid ssmId;								///< SSM管理ID
	bool isVerbose;								///< エラーメッセージを出力するかどうか
	bool isBlocking;							///< ブロッキングするかどうか
	void *mData;								///< データのポインタ
	uint64_t mDataSize;							///< データ構造体のサイズ
	void *mProperty;							///< プロパティのポインタ
	uint64_t mPropertySize;						///< プロパティサイズ

	/**
	 * @brief 初期化
	 */
	void init(  )
	{
		streamName = "";
		streamId = 0;
		ssmId = 0;
		isVerbose = true;
		isBlocking = false;
		timeId = -1;
		mData = NULL;
		mDataSize = 0;
		mProperty = NULL;
		mPropertySize = 0;
	}
	/**
	 * @brief 名前の設定
	 */
	void init( const char *streamName, int streamId = 0 )
	{
		init(  );
		this->streamName = streamName;
		this->streamId = streamId;
	}
	

	/**
	 * @brief 接続するSSMストリームの設定
	 * @param[in] streamName ストリーム名
	 * @param[in] streamId	ストリームの番号
	 */
	void setStream( const char *streamName, int streamId = 0 )
	{
		this->streamName = streamName;
		this->streamId = streamId;
	}

public:
	SSM_tid timeId;								///< データのTimeID
	ssmTimeT time;								///< データのタイムスタンプ

	/**
	 * @overload
	 */
	SSMApiBase(  )
	{
		init(  );
	}

	/**
	 * @brief コンストラクタ
	 * @param[in] streamName ストリーム名
	 * @param[in] streamId	ストリームの番号
	 */
	SSMApiBase( const char *streamName, int streamId = 0 )
	{
		init(  );
		setStream( streamName, streamId );
	}
	
	virtual ~SSMApiBase(){}

	/**
	 * @brief 開いているかどうかを返す
	 */
	bool isOpen(  ) const
	{
		return ( ssmId != 0 ? true : false );
	}
	/**
	 * @brief 更新されたかどうかを返す
	 */
	bool isUpdate(  )
	{
		if( !isOpen(  ) )
		{
			return false;
		}
		return ( getTID_top( ssmId ) > timeId );
	}

	/**
	 * @brief エラーメッセージを出力するかどうか
	 */
	void setVerbose( bool verbose )
	{
		isVerbose = verbose;
	}
	
	/**
	 * @brief ブロッキングするかどうか
	 * 
	 * ブロッキングは使用が変更される可能性があるので、使用は上級者向け。
	 * これを設定すると、readNew及びreadNextがブロッキングされる
	 */
	void setBlocking( bool isBlocking )
	{
		this->isBlocking = isBlocking;
	}

	/**
	 * @brief get ssm stream name.
	 */
	const char *getStreamName() const
	{
		return streamName;
	}
	const char *getSensorName() const
	{
		std::cerr << "SSMBase::getSensorName is old function. so please use get SSMBase::getStreamName()" << std::endl;
		return streamName;
	}
	
	/**
	 * @brief get ssm stream id.
	 */
	int getStreamId() const
	{
		return streamId;
	}
	int getSensorId() const
	{
		std::cerr << "SSMBase::getSensorId is old function. so please use get SSMBase::getStreamId()" << std::endl;
		return streamId;
	}
	
	/**
	 * @brief get ssm id.
	 */
	SSM_sid getSSMId() const
	{
		return ssmId;
	}
	
	void *data()
	{
		return mData;
	}
	
	uint64_t dataSize()
	{
		return mDataSize;
	}
	
	void *property()
	{
		return mProperty;
	}
	
	uint64_t propertySize()
	{
		return mPropertySize;
	}

	/// 共有メモリに確保するデータ構造体のサイズ
	virtual uint64_t sharedSize(  )
	{
		return mDataSize;
	}
	
	/**
	 * @brief ストリームの作成
	 *
	 * create()したら、終了時に destroy() すること。
	 *
	 * @see createSSM, destroy, open
	 */
	bool create( double saveTime, double cycle )
	{
		if( !mDataSize )
		{
			std::cerr << "SSM::create() : data buffer of ''" << streamName << "', id = " << streamId << " is not allocked." << std::endl;
			return false;
		}

		ssmId = createSSM( streamName, streamId, sharedSize(  ), saveTime, cycle );
                
		if( ssmId == 0 )
		{
			if( isVerbose )
			{
				std::cerr << "SSM::create() : cannot create '" << streamName << "', id = " << streamId << std::endl;
			}
			return false;
		}
		return true;
	}

	/**
	 * @overload
	 * @see create
	 */
	bool create( const char *streamName, int streamId, double saveTime, double cycle )
	{
		setStream( streamName, streamId );
		return create( saveTime, cycle );
	}

	/**
	 * @brief ストリームの破棄
	 * @see releaseSSM, create
	 */
	bool release(  )
	{
		return releaseSSM( &ssmId );
	}

	/**
	 * @overload
	 * @see open
	 */
	bool open( SSM_open_mode openMode = SSM_READ )
	{
		if( !mDataSize )
		{
			std::cerr << "SSM::open() : data buffer of '" << streamName << "', id = " << streamId << " is not allocked." << std::endl;
			return false;
		}
		ssmId = openSSM( streamName, streamId, openMode );
		if( ssmId == 0 )
		{
			if( isVerbose )
			{
				std::cerr << "SSM::open() : cannot open '" << streamName << "', id = " << streamId << std::endl;
			}
			return false;
		}
		return true;
	}

	/**
	 * @brief ストリームを開く
	 * @param[in] streamName ストリーム名
	 * @param[in] streamId ストリームの番号
	 * @param[in] openMode ストリームのオープンモード選択フラグ
	 *
	 * open() したら必ず close() すること！
	 *
	 * open() では、read,writeのフラグを設定する。
	 *
	 * @see openSSM, openWait
	 */
	bool open( const char *streamName, int streamId = 0, SSM_open_mode openMode = SSM_READ )
	{
		setStream( streamName, streamId );
		return open( openMode );
	}

	/**
	 * @brief 開くまで待つ
	 * @param[in] timeOut (sec)タイムアウトの時間。0以下だとタイムアウトしない
	 *
	 * @see open
	 */
	bool openWait( ssmTimeT timeOut = 0.0, SSM_open_mode openMode = SSM_READ )
	{
		if( !mDataSize )
			return false;

		bool ret, isVerbose;
		ssmTimeT start = gettimeSSM_real(  );

		isVerbose = this->isVerbose;
		this->isVerbose = false;

		ret = open( openMode );
		if( isVerbose && ( !ret ))
		{
			std::cerr << "SSM::openWait() : wait for stream '" << streamName << "', id = " << streamId << std::endl;
		}

		while( ( !ret ) && ( !sleepSSM( 1.0 ) ) && ( ( timeOut <= 0 ) || gettimeSSM_real(  ) - start < timeOut ) )
		{
			ret = open(  );
		}

		this->isVerbose = isVerbose;
		if( !ret )
			ret = open( openMode );
		return ret;
	}
	
	/**
	 * @overload
	 */
	bool openWait( const char *streamName, int streamId = 0, ssmTimeT timeOut = 0.0, SSM_open_mode openMode = SSM_READ )
	{
		setStream( streamName, streamId );
		return openWait( timeOut, openMode );
	}

	/**
	 * @brief ssmに書き込み読み込みをできなくする
	 * @see closeSSM
	 */
	bool close(  )
	{
		return closeSSM( &ssmId );
	}

	/**
	 * @brief timeを指定して書き込み
	 * @param[in] time 時間。指定しないときは現在時刻を書き込み
	 * @return 正しく書き込めたときtrueを返す
	 *
	 * @see writeSSM_time, read
	 */
	virtual bool write( ssmTimeT time = gettimeSSM(  ) )
	{
		SSM_tid tid = writeSSM( ssmId, mData, time );
		if( tid >= 0 )
		{
			timeId = tid;
			this->time = time;
			return true;
		}
		return false;
	}
	/**
	 * @brief tidを指定して読み込み
	 * @return 正しく読み込めたときtrueを返す
	 *
	 * @see readSSM, readTime
	 */
	virtual bool read( SSM_tid timeId = -1 )
	{
		SSM_tid tid = readSSM( ssmId, mData, &time, timeId );
		if( tid >= 0 )
		{
			this->timeId = tid;
			return true;
		}
		return false;
	}

	/** 
	 * @brief 前回読み込んだデータの次のデータを読み込む
	 * @param[in] dt 進む量
	 * @return 次データを読み込めたときtrueを返す
	 * 
	 * 指定するデータがSSMの保存しているデータよりも古いとき、
	 * 保存されている中で最も古いデータを読み込む
	 * @see read, readBack, readNew, readLast, readTime
	 */
	bool readNext( int dt = 1 )
	{
		if( !isOpen(  ) )
		{
			return false;
		}
		SSM_tid tid = getTID_bottom( ssmId ), rtid;
		if( timeId < 0 )
		{
			if( isBlocking )
				waitTID( ssmId, 0 );
			return read( -1 );
		}
		rtid = timeId + dt;
		if( rtid >= tid)
		{
			if( isBlocking )
				waitTID( ssmId, rtid );
			return read( rtid );
		}

		if( isVerbose )
			std::cerr << "SSM::readNext() : skipping read data  '" << streamName << "', id = " << streamId
				<< " TID: "<< rtid << " -> " << tid << std::endl;
		return read( tid );
	}

	/**
	 * @brief 前回のデータの1つ前のデータを読み込む
	 * @param[in] dt 戻る量（正の値）
	 * @return 次データを読み込めたときtrueを返す
	 *
	 * @see read, readNext, readNew, readLast, readTime
	 */
	bool readBack( int dt = 1 )
	{
		return ( dt <= timeId ? read( timeId - dt ) : false );
	}

	/**
	 * @brief 最新データの読み込み
	 * @return 正しく読み込めたときtrueを返す
	 *
	 * @see read, readNew, readNext, readBack, readTime
	 */
	bool readLast(  )
	{
		return read( -1 );
	}

	/**
	 * @brief 新しいデータの読み込み
	 * @return 最新であり前回と違うときtrueを返す。
	 *
	 * @see read, readLast, readNext, readBack, readTime
	 */
	bool readNew(  )
	{
		if( isBlocking )
			waitTID( ssmId, timeId + 1 );
		return ( isUpdate(  ) ? read( -1 ) : false );
	}

	/**
	 * @brief 時間指定読み込み
	 * @return 正しく読み込めたときtrueを返す
	 *
	 * @see readSSM_time, read
	 */
	virtual bool readTime( ssmTimeT time )
	{
		SSM_tid tid = readSSM_time( ssmId, mData, time, &( this->time ) );
		if( tid >= 0 )
		{
			timeId = tid;
			return true;
		}
		return false;
	}
	
	/**
	 * @brief プロパティの設定
	 * @return 書き込めたときtrueを返す
	 *
	 * @see set_propertySSM, getProperty
	 */
	bool setProperty(  )
	{
		if( mPropertySize > 0 )
			return static_cast < bool > ( set_propertySSM( streamName, streamId, mProperty, mPropertySize ) );
		else
			return false;
	}

	/**
	 * @brief プロパティの取得
	 * @return 読み込めたときtrueを返す
	 *
	 * @see get_propertySSM, setProperty
	 */
	bool getProperty(  )
	{
		if( mPropertySize > 0 )
			return static_cast < bool > ( get_propertySSM( streamName, streamId, mProperty ) );
		else
			return false;
	}

	/**
	 * @brief バッファの設定
	 *
	 * 継承したときなど、ポインタを外部からさわらせたくないときはprivateの中に入れる
	 */
	void setBuffer(void *data, uint64_t dataSize, void *property, uint64_t propertySize )
	{
		mData = data;
		mDataSize = dataSize;
		mProperty = property;
		mPropertySize = propertySize;
	}

	/**
	 * データだけのバッファをセットする
	 */
	void setDataBuffer(void *data, uint64_t dataSize) {
		mData = data;
		mDataSize = dataSize;
	}

	/**
	 * プロパティのバッファをセットする
	 */
	void setPropertyBuffer(void *property, uint64_t propertySize) {
		mProperty = property;
		mPropertySize = propertySize;
	}



	/**
	 * 共有メモリの中身を見る．基本的にはデバッグ用
	 *
	 */
	void showRawData() {
		ssm_header *shm_p = NULL;
		//SSM_tid tid = writeSSM( ssmId, mData, time );
		shm_p = shm_get_address(ssmId);
		printf("--- show row data ---\n");
		printf("size = 0x%llx\n", shm_p->size);
		printf("num = %d\n", shm_p->num);
		char *p = NULL;
		for (int i = 0; i < shm_p->num; ++i) {
			p = (char*)shm_get_data_ptr(shm_p, i);
			printf("%d = %p\n", i, p);
			for (int j = 0; j < 8; j++) {
				printf("%02x ", p[j] & 0xff);
			}
			printf("\n");
		}
		printf("\n----- ssm time ------\n");
		ssmTimeT *tp = shm_get_time_address(shm_p);
		for (int i = 0; i < shm_p->num; ++i) {
			p = (char*)&tp[i];
			printf("%d = %p\n", i, p);
			for (int j = 0; j < 8; ++j) {
				printf("%02x ", p[j] & 0xff);
			}
			printf("\n");
		}




	}

};

/**
 * @brief SSMを簡単に扱うためのクラス
 *
 * 基本的にこれを使う
 */
template < typename T, typename P = SSMDummy > class SSMApi:public SSMApiBase
{
	void initApi()
	{
		SSMApiBase::setBuffer( &data, sizeof( T ), &property, sizeof( P ) );
	}

protected:
	void setBuffer(void *data, uint64_t dataSize, void *property, uint64_t propertySize );

public:
	T data;										 ///< @brief SSM Data.
	P property;									 ///< @brief Property of SSM Data.

	/**
	 * コンストラクタ
	 */
	SSMApi(  )
	{
		initApi();
	}
 	
	/**
	 * @overload
	 */
	SSMApi( const char *streamName, int streamId = 0 ) : SSMApiBase( streamName, streamId )
	{
		initApi();
	}
	
	/**
	 * デストラクタ
	 */
	~SSMApi(  ){}
};

#endif											/* __SSM_HPP__ */
