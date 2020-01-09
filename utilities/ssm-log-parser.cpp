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

	std::fstream *mLogFile;						///< ログファイル
  std::fstream *mOutFile;           ///< パース後のログファイル
	std::string mStreamName;					///< ストリーム名
	std::ios::pos_type mStartPos;				///< logの開始位置
	std::ios::pos_type mEndPos;					///< logの終了位置
	std::ios::pos_type mPropertyPos;			///< propertyの書き込み位置
  std::unique_ptr<char[]> p;

  void init() {
    mLogFile = nullptr;
		mData = nullptr;
		mDataSize = 0;
		mProperty = nullptr;
		mPropertySize = 0;
		mStreamId = 0;
		mBufferNum = 0;
		mCycle = 0.0;
		mStartTime = 0.0;
		mStartPos = 0;
		mEndPos = 0;
		mPropertyPos = 0;
    writeCnt = 0;
  }

  bool getLogInfo() {
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
    std::cout << mPropertySize << " " << mDataSize << "\n";
    mProperty = new char[mPropertySize];
    mData = new char[mDataSize];

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

  /**
	 * @brief プロパティを読み込む
	 */
	bool readProperty() {
		int curPos;
		curPos = mLogFile->tellg();

		mLogFile->seekg( mPropertyPos, std::ios::beg );
		mLogFile->read( (char *)mProperty, mPropertySize );

		mLogFile->seekg( curPos, std::ios::beg );
		return true;
	}

  /**
   * @brief ログデータを読み出す。
   */
  bool read() {
		mLogFile->read( (char *)&mTime, sizeof( ssmTimeT ) );
		mLogFile->read( mData, mDataSize );
		return mLogFile->good(  );
	}

public:
  SSMLogParser() {
    this->init();
  }

  ~SSMLogParser() {
    mLogFile->close();
    mOutFile->close();
    delete mLogFile;
    delete mOutFile;
  }


	/**
	 * @brief ログファイルを開く
	 */
	bool open( std::string fileName ) {
		mLogFile = new std::fstream(  );
		mLogFile->open( fileName, std::ios::in | std::ios::binary );
		if( !mLogFile->is_open(  ) ) {
			delete mLogFile;
			std::cerr << "SSMLogParser::open : cannot open log file '" << fileName << "'." << std::endl;
			return false;
		}
		if( !getLogInfo(  ) )
		{
			std::cerr << "SSMLogParser::open : '" << fileName << "' is NOT ssm-log file." << std::endl;
			return false;
		}
		return true;
	}

  /**
   * @brief ログパース後ファイルを作る
   */
  bool create(std::string fileName, uint32_t cnt) {
    mOutFile = new std::fstream();
    mOutFile->open(fileName + "out.txt", std::ios::out);
    *mOutFile << std::setprecision(15);
    if (!mOutFile->is_open()) {
      std::cerr << "SSMLogParser::create: can't create file" << std::endl;
      return false;
    }
    this->writeCnt = cnt;
    this->p.reset(new char[this->writeCnt]);
    return true;
  }

  bool readNByte() {
    if (!this->read()) {
			std::cerr << "end\n";
      return false;
    }

    *mOutFile << mTime << "\n";
    for (auto i = 0; i < std::min((uint64_t) writeCnt, mDataSize); ++i) {
      // if (i % 16 == 0) printf("\n");
      // printf("%02X ", ((char *)mData)[i] & 0xff);
      *mOutFile << (((char *)mData)[i] & 0xff) << " ";
    }
    // printf("\n");
    *mOutFile << "\n";
    return true;
  }
};

int main(int argc, char* argv[]) {
  std::string src {argv[1]};
  std::string dist {argv[2]};
  SSMLogParser parser;
  parser.open(src);
  parser.create(dist, 8);
	int cnt = 0;
  while (parser.readNByte()) {cnt++;}
	std::cerr << cnt << "\n";
}