#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <cmath>
#include <iomanip>

#include "ssm-log-parser.hpp"

using std::cout;
using std::endl;

void SSMLogParser::init() {
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

bool SSMLogParser::getLogInfo() {
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
		<< "Stream Name: " << mStreamName << "\n"
		<< "Stream Id: " << mStreamId << "\n"
		<< "DataSize: " << mDataSize << "\n"
		<< "BufferNum: " << mBufferNum << "\n"
		<< "Cycle: " << mCycle << "\n"
		<< "Start Time: " << mStartTime << "\n"
		<< "Property Size: " << mPropertySize
		<< std::endl;
#endif
  // std::cout << mPropertySize << " " << mDataSize << "\n";
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
bool SSMLogParser::readProperty() {
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
bool SSMLogParser::read() {
	mLogFile->read( (char *)&mTime, sizeof( ssmTimeT ) );
	mLogFile->read( mData, mDataSize );
	return mLogFile->good(  );
}

void SSMLogParser::writeStreamInfo() {
	*mOutFile 
		<< "Stream Name: " << mStreamName << "\n"
		<< "Stream Id: " << mStreamId << "\n"
		<< "DataSize: " << mDataSize << "\n"
		<< "BufferNum: " << mBufferNum << "\n"
		<< "Cycle: " << mCycle << "\n"
		<< "Start Time: " << mStartTime << "\n"
		<< "Property Size: " << mPropertySize << "\n\n"
		<< "----start----\n"
		<< std::endl; // flush
}

/**
 * Setter
 */
void SSMLogParser::setIpAddress(std::string address) {
	this->mIpAddress = address;
}

void SSMLogParser::setData(char *ptr) {
	this->mData = ptr;
}

// Setter終わり

/**
 * Getter. C#のプロパティのように取得できる。
 */
ssmTimeT SSMLogParser::time() {
	return this->mTime;
}

char *SSMLogParser::data() {
	return this->mData;
}

uint64_t SSMLogParser::dataSize() {
	return this->mDataSize;
}

char *SSMLogParser::property() {
	return this->mProperty;
}

uint64_t SSMLogParser::propertySize() {
	return this->mPropertySize;
}

void *SSMLogParser::fullData() {
	return this->mFullData;
}

std::string SSMLogParser::streamName() {
	return this->mStreamName;
}

int SSMLogParser::streamId() {
	return this->mStreamId;
}

int SSMLogParser::bufferNum() {
	return this->mBufferNum;
}

double SSMLogParser::cycle() {
	return this->mCycle;
}

ssmTimeT SSMLogParser::startTime() {
	return this->mStartTime;
}

std::string SSMLogParser::ipAddress() {
	return this->mIpAddress;
}

// Getter終わり

/**
 * Constructors
 */

SSMLogParser::SSMLogParser() {
  this->init();
}

SSMLogParser::SSMLogParser(std::string src) {
	this->init();
	this->open(src);
}

SSMLogParser::SSMLogParser(std::string src, std::string ipAddress) : mIpAddress(ipAddress) {
	cout << src << " "<< ipAddress << endl;
	this->init();
	this->open(src);
}

/**
 * Move constructor
 */

SSMLogParser::SSMLogParser(SSMLogParser&& rhs) {
	this->p = std::move(rhs.p); // after move, called rhs.p.release();
}

/**
 * Destructors
 */
SSMLogParser::~SSMLogParser() {
	if (mLogFile != nullptr && mLogFile->is_open()) { mLogFile->close(); }
  if (mOutFile != nullptr && mOutFile->is_open()) { mOutFile->close(); }
  delete mLogFile;
  delete mOutFile;
}


/**
 * @brief ログファイルを開く
 */
bool SSMLogParser::open( std::string fileName ) {
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
bool SSMLogParser::create(std::string fileName, uint32_t cnt) {
  mOutFile = new std::fstream();
  mOutFile->open(fileName + "out.txt", std::ios::out);
  *mOutFile << std::setprecision(15);
  if (!mOutFile->is_open()) {
    std::cerr << "SSMLogParser::create: can't create file" << std::endl;
    return false;
	}

  this->writeCnt = cnt;
  this->p.reset(new char[this->writeCnt]);
	writeStreamInfo();
  return true;
}

bool SSMLogParser::readNByte() {
  if (!this->read()) {
		std::cerr << "end\n";
    return false;
  }

  *mOutFile << mTime << "\n";
  for (auto i = 0; i < std::min((uint64_t) writeCnt, mDataSize); ++i) {
    if (i % 16 == 0) printf("\n");
    printf("%02X ", ((char *)mData)[i] & 0xff);
    *mOutFile << (((char *)mData)[i] & 0xff) << " ";
  }
  *mOutFile << "\n";
  return true;
}

/*int main(int argc, char* argv[]) {
	if (argc < 4) {
		std::cerr
			<< "input: <src_filename:string>"
			<< "<dist_filename:string>"
			<< "<cnt:uint32_t>"
			<< std::endl;
			return 0;
	}
  std::string src = argv[1];
  std::string dist = argv[2];
	std::istringstream iss(argv[3]);
	uint32_t writeCnt = 0;
	iss >> writeCnt;
  SSMLogParser parser;
  parser.open(src);
  parser.create(dist, writeCnt);
	int cnt = 0;
  while (parser.readNByte()) {cnt++;}
	std::cerr << cnt << "\n";
}*/