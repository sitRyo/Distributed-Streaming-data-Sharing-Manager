#ifndef __INC_SSM_LOG_PARSER__
#define __INC_SSM_LOG_PARSER__

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <cmath>
#include <iomanip>

using ssmTimeT = double;

class SSMLogParser {
private: 
	ssmTimeT mTime;	
	char *mData;								///< データのポインタ
	uint64_t mDataSize;							///< データ構造体のサイズ
	char *mProperty;							///< プロパティのポインタ
	uint64_t mPropertySize;						///< プロパティサイズ
	void *mFullData;
	int mStreamId;								///< ストリームID
	int mBufferNum;								///< ssmのリングバッファの個数
	double mCycle;								///< streamへの書き込みサイクル
	ssmTimeT mStartTime;						///< logを書き込み始めた時間
  uint32_t writeCnt;
  std::string mIpAddress;

	std::fstream *mLogFile;						///< ログファイル
  std::fstream *mOutFile;           ///< パース後のログファイル
	std::string mStreamName;					///< ストリーム名
	std::ios::pos_type mStartPos;				///< logの開始位置
	std::ios::pos_type mEndPos;					///< logの終了位置
	std::ios::pos_type mPropertyPos;			///< propertyの書き込み位置
  std::unique_ptr<char[]> p;

  void init();

  bool getLogInfo();
  /**
	 * @brief プロパティを読み込む
	 */
	bool readProperty();

  /**
   * @brief ログデータのヘッダを書き出す。
   */
	void writeStreamInfo();

public:
  SSMLogParser();
  SSMLogParser(std::string src);
  SSMLogParser(std::string src, std::string ipAddress);
  SSMLogParser(SSMLogParser&& rhs);
  ~SSMLogParser();

  /* Getter */

  ssmTimeT time();
  char *data();
  uint64_t dataSize();
  char *property();
  uint64_t propertySize();
	void *fullData();
  std::string streamName();
	int streamId();					
	int bufferNum();				
	double cycle();					
	ssmTimeT startTime();		
  std::string ipAddress();

  /* Setter */

  void setIpAddress(std::string address);
  void setData(char *ptr);

	/**
	 * @brief ログファイルを開く
	 */
	bool open(std::string fileName);

  /**
   * @brief ログデータを読み出す。
   */
  bool read();

  /**
   * @brief ログパース後ファイルを作る
   */
  bool create(std::string fileName, uint32_t cnt);

  /**
   * @brief 先頭Nバイトを読み出す。
   */
  bool readNByte();
};


#endif // __INC_SSM_LOG_PARSER_