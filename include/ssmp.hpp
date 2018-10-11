#ifndef __SSMP_HPP__
#define __SSMP_HPP__
#include "ssm.hpp"
#include <cstdlib>

#define SSMApiP SSMPApi
/**
 * SSM with Pointer
 */ 
template <typename T, typename P = SSMDummy>
class SSMPApi : public SSMApiBase
{
	void setBuffer(void *data, size_t dataSize, void *property, size_t propertySize );

	void initApi()
	{
		data = NULL;
		writeData = NULL;
		readData = NULL;
		userData = NULL;

		SSMApiBase::setBuffer( NULL, 0, &property, sizeof( P ) );

	}
public:
	T *data;            // @brief SSM Data.
	P property;        // @brief Property of SSM Data.
	void ( *writeData )( void *ssmp, const void *data, void *userData );
	void ( *readData )( const void *ssmp, void *data, void *userData );
	void *userData;

	
	SSMPApi(){ initApi(); }
	SSMPApi(const char *streamName, int streamId = 0): SSMApiBase( streamName, streamId ) { initApi(); }
	~SSMPApi(){ free(data); }
	
	// @brief data格納用メモリの確保
	// @param dataSize[in] データのサイズ
	// @retrun メモリ確保に成功したらtrueを返す
	bool alloc( size_t dataSize )
	{
		bool ret = true;
		data = (T*)malloc( dataSize );
		
		if( data )
			SSMApiBase::setBuffer( data, dataSize, &property, sizeof( P ) );
		else
			ret = false;
		
		return ret;
	}


	// @brief timeを指定して書き込み
	// @param time[in] 時間。指定しないときは現在時刻を書き込み
	// @return 正しく書き込めたときtrueを返す
	bool write( ssmTimeT time = gettimeSSM() )
	{
		int tid;
		if( writeData )
			tid = writeSSMP( ssmId, data, time, writeData, userData );
		else
			tid = writeSSM( ssmId, data, time );
		if(tid >= 0){timeId = tid; this->time = time; return true;}
		return false;
	}

	// @brief tidを指定して読み込み
	// @return 正しく読み込めたときtrueを返す
	bool read(int timeId = -1)
	{
		int tid;
		if( readData )
			tid = readSSMP(ssmId, data, &time, timeId, readData, userData );
		else
			tid = readSSM(ssmId, data, &time, timeId);
		if(tid >= 0){this->timeId = tid; return true;}
		return false;
	}
};

#endif //__SSMP_HPP__


