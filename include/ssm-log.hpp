#ifndef SSM_LOG_HPP__
#define SSM_LOG_HPP__

#include <fstream>
#include <string>
#include <sstream>
#include <cstring>

#include <ssm.hpp>

/**
 * @brief SSMのログにアクセスするためのクラス
 */
class SSMLogBase
{
protected:
	ssmTimeT mTime;	
	void *mData;								///< データのポインタ
	uint64_t mDataSize;							///< データ構造体のサイズ
	void *mProperty;							///< プロパティのポインタ
	uint64_t mPropertySize;						///< プロパティサイズ
	void *mFullData;
	int mStreamId;								///< ストリームID
	int mBufferNum;								///< ssmのリングバッファの個数
	double mCycle;								///< streamへの書き込みサイクル
	ssmTimeT mStartTime;						///< logを書き込み始めた時間

	std::fstream *mLogFile;						///< ログファイル
	std::string mStreamName;					///< ストリーム名
	std::ios::pos_type mStartPos;				///< logの開始位置
	std::ios::pos_type mEndPos;					///< logの終了位置
	std::ios::pos_type mPropertyPos;			///< propertyの書き込み位置
	
	void init()
	{
		mLogFile = NULL;
		mData = NULL;
		mDataSize = 0;
		mProperty = NULL;
		mPropertySize = 0;
		mStreamId = 0;
		mBufferNum = 0;
		mCycle = 0.0;
		mStartTime = 0.0;
		mStartPos = 0;
		mEndPos = 0;
		mPropertyPos = 0;
	}

	bool writeProperty()
	{
		int curPos;
		mLogFile->clear();
		curPos = mLogFile->tellp();

		mLogFile->clear();
		mLogFile->seekp( mPropertyPos, std::ios::beg );
		mLogFile->write( (char *)mProperty, mPropertySize );
		
		mLogFile->clear();
		mLogFile->seekp( curPos, std::ios::beg );

		return true;
	}

	bool getLogInfo()
	{
		std::string line;
		mLogFile->seekg( 0, std::ios::beg );
		if( !getline( *mLogFile, line ) )
			return false;
		std::istringstream data( line.c_str(  ) );

		data
			>> mStreamName
			>> mStreamId
			>> mDataSize
			>> mBufferNum
			>> mCycle
			>> mStartTime
			>> mPropertySize
			;
		if( data.fail() )
			return false;
#if 0
		std::cout
			<< mStreamName << ' '
			<< mStreamId << ' '
			<< mDataSize << ' '
			<< mBufferNum << ' '
			<< mCycle << ' '
			<< mStartTime << ' '
			<< mPropertySize
			<< std::endl;
#endif
		// プロパティの読み込み
		mPropertyPos = mLogFile->tellg();
		if( mProperty )
			readProperty();
		mLogFile->seekg( mPropertySize, std::ios::cur );
		
		// ログ位置の保存
		mStartPos = mLogFile->tellg();
		mLogFile->seekg(0, std::ios::end );
		mEndPos = mLogFile->tellg();
		mLogFile->seekg( mStartPos );
		
		mTime = mStartTime;
				
		return true;
	}

	bool setLogInfo( const ssmTimeT &startTime )
	{
		mLogFile->setf(std::ios::fixed);
		mLogFile->seekp( 0, std::ios::beg );
		mStartTime = startTime;
		*mLogFile
			<< mStreamName << ' '
			<< mStreamId << ' '
			<< mDataSize << ' '
			<< mBufferNum << ' '
			<< mCycle << ' '
			<< mStartTime << ' '
			<< mPropertySize
			<< std::endl
			;

		// プロパティの書き込み
		mPropertyPos = mLogFile->tellp();
		writeProperty();
		mLogFile->seekp( mPropertySize, std::ios::cur );
		// ログ開始位置の保存
		mStartPos = mLogFile->tellp();
		
		mLogFile->seekp( mStartPos );
		return true;		
	}

public:
	/// コンストラクタ
	SSMLogBase()
	{
		init();
	}

	/// デストラクタ
	virtual ~SSMLogBase()
	{
	}
	void setBuffer(void *data, uint64_t dataSize, void *property, uint64_t propertySize, void *fulldata)
	{
		mData = data;
		mDataSize = dataSize;
		mProperty = property;
		mPropertySize = propertySize;
		mFullData = fulldata;
	}

	/**
	 * @brief プロパティを読み込む
	 */
	bool readProperty()
	{
		int curPos;
		curPos = mLogFile->tellg();

		mLogFile->seekg( mPropertyPos, std::ios::beg );
		mLogFile->read( (char *)mProperty, mPropertySize );

		mLogFile->seekg( curPos, std::ios::beg );
		return true;
	}
	
	/**
	 * @brief ログファイルを開く
	 */
	bool open( const char *fileName )
	{
		mLogFile = new std::fstream(  );
		mLogFile->open( fileName, std::ios::in | std::ios::binary );
		if( !mLogFile->is_open(  ) )
		{
			delete mLogFile;
			std::cerr << "SSMLogBase::open : cannot open log file '" << fileName << "'." << std::endl;
			return false;
		}
		if( !getLogInfo(  ) )
		{
			std::cerr << "SSMLogBase::open : '" << fileName << "' is NOT ssm-log file." << std::endl;
			return false;
		}
		return true;
	}
	
	/**
	 * @brief ログファイルを作成する
	 */
	bool create( const char *streamName, int streamId, int bufferNum, double cycle, const char *fileName, ssmTimeT startTime = gettimeSSM(  ) )
	{
		mLogFile = new std::fstream(  );
		mLogFile->open( fileName, std::ios::out | std::ios::trunc | std::ios::binary );
		mStreamName = streamName;
		mStreamId = streamId;
		mBufferNum = bufferNum;
		mCycle = cycle;
		if( !mLogFile->is_open(  ) )
		{
			std::cerr << "SSMLogBase::create() : cannot create log file '" << fileName << "'." << std::endl;
			return false;
		}
		if( !setLogInfo( startTime ) )
		{
			std::cerr << "SSMLogBase::create() : cannot write ssm-info to '" << fileName << "'." << std::endl;
			this->close(  );
			return false;
		}
		return true;
	}
	
	/**
	 * @brief ログファイルを閉じる
	 */
	bool close(  )
	{
		delete mLogFile;
		mLogFile = NULL;
		return true;
	}
	
	virtual bool write( ssmTimeT time = gettimeSSM(  ) )
	{
		mTime = time;
		mLogFile->write( (char *)&mTime, sizeof( ssmTimeT ) );
		mLogFile->write( (char *)mData, mDataSize );
		return mLogFile->good(  );
	}
	
	virtual bool read(  )
	{
		mLogFile->read( (char *)&mTime, sizeof( ssmTimeT ) );
		mLogFile->read( (char *)mData, mDataSize );
		return mLogFile->good(  );
	}
	
	virtual bool readFull() {
            double mlTime;
            mLogFile->read((char*)mFullData, sizeof(ssmTimeT) + mDataSize);
            mlTime = *(reinterpret_cast<double*>(mFullData));
            char *p = &((char*)mFullData)[8];		
            return mLogFile->good(  );
	}

	
	/**
	 * @brief 指定したデータ数分シークする
	 *
	 * read時専用
	 */
	bool seek( int diff )
	{
		mLogFile->clear(  ); // eofになっているとseekできないので
		std::fstream::pos_type pos,curPos = mLogFile->tellg();
		// シーク
		mLogFile->seekg( static_cast<int>( sizeof( ssmTimeT ) + mDataSize ) * diff, std::ios::cur );
		// シークに成功したかをチェック
		pos = mLogFile->tellg( );
		if( pos < mStartPos || pos > mEndPos )
		{
			// シーク失敗なら終了
			mLogFile->seekg( curPos );
			return false;
		}
		return true;
	}
		
	/**
	 * @brief 時間指定読み込み
	 * 
	 * 指定した時間より前で最も近いデータを読み込む
	 */
	bool readTime( ssmTimeT time )
	{
		std::ios::pos_type curPos = mLogFile->tellg();
		// cycleを使って予測ジャンプ
		seek( ( time - mTime ) / mCycle );
		// 現在位置を読み込み
		readNext() || readBack(); // 一番最後だと読み込めないので１つ戻って読み込む
		
		// 条件を満たすまで探索
		mLogFile->clear(  );
		while( mTime < time && readNext(  ) );
		mLogFile->clear(  );
		while( mTime >= time && readBack(  ) );
		
		return true;
	}

	const char *getStreamName( ) const
	{
		return mStreamName.c_str();
	}

	bool readNext(  )
	{
		return read(  );
	}

	bool readBack(  )
	{
		return seek( -2 ) && read();
	}

	const ssmTimeT &getStartTime( ) const
	{
		return mStartTime;
	}

	ssmTimeT &time(  )
	{
		return mTime;
	}
	void *data(  )
	{
		return mData;
	}
	uint64_t &dataSize(  )
	{
		return mDataSize;
	}
	
	void *property(  )
	{
		return mProperty;
	}
	uint64_t &propertySize()
	{
		return mPropertySize;
	}
	
	
	const int &getStreamId(  ) const { return mStreamId; }
	
	const double &getCycle(  ) const { return mCycle; }
	
	const int &getBufferNum(  ) const { return mBufferNum; }
};

class SSMNetworkLogBase : public SSMLogBase {
public:
	SSMNetworkLogBase() {
	}

	~SSMNetworkLogBase() {
	}

	bool read(  )
	{
		printf("hogehoge\n");
		mLogFile->read( (char *)&mTime, sizeof( ssmTimeT ) );
		mLogFile->read( (char *)mData, mDataSize );
		return mLogFile->good(  );
	}
};




/**
 * @brief SSMのログにアクセスするためのクラス
 *
 * SSMApiと同じ感じで使えるように設計した
 */
template < typename D, typename P = SSMDummy > class SSMLog : public SSMLogBase
{
	void setBuffer(void *data, uint64_t dataSize, void *property, uint64_t propertySize );
	void initLog()
	{
		SSMLogBase::setBuffer( &dataBuf, sizeof( D ), &propertyBuf, sizeof( P ) );
	}
	D dataBuf;										// @brief SSM Data.
	P propertyBuf;									// @brief Property of SSM Data.
public:

	SSMLog(  )
	{
		initLog();
	}
	
	virtual ~SSMLog(  ){}
	
	D &data(  )
	{
		return dataBuf;
	}
	
	P &property(  )
	{
		return propertyBuf;
	}
};
#endif // SSM_LOG_H__
