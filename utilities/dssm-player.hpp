#ifndef __INC_SSM_LOG_PARSER__
#define __INC_SSM_LOG_PARSER__

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <cmath>
#include <iomanip>
#include <vector>
#include <ssm.hpp>
#include <ssm-proxy-client.hpp>

using ssmTimeT = double;

struct DataReader {
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
  std::unique_ptr<std::fstream> mLogFile;
  std::unique_ptr<std::string> mIpAddr;
  std::string mLogSrc;
  std::unique_ptr<SSMApiBase> ssmApi;
  std::unique_ptr<PConnector> con;

  char *fulldata; // for PConnector

  std::string mStreamName;					///< ストリーム名
	std::ios::pos_type mStartPos;				///< logの開始位置
	std::ios::pos_type mEndPos;					///< logの終了位置
	std::ios::pos_type mPropertyPos;			///< propertyの書き込み位置
  std::unique_ptr<char[]> p;

  DataReader(std::string src, std::string ipAddr = "empty");
  DataReader(DataReader const& rhs);
  DataReader(DataReader&& rhs) noexcept;
  ~DataReader();
  void init();
  bool getLogInfo();
  bool readProperty();
  bool read();
};

class SSMLogParser {
private: 
	std::vector<DataReader> mLogFile;						///< ログファイル
  std::vector<std::fstream> mOutFile;           ///< パース後のログファイル
	std::vector<std::string> streamArray;

public:
  SSMLogParser();
  // SSMLogParser(SSMLogParser&& rhs);
  ~SSMLogParser();

  bool optAnalyze(int aArgc, char **aArgv);

	bool open();
  bool create();
  bool readNByte();
};


#endif // __INC_SSM_LOG_PARSER_