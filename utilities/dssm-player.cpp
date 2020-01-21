#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <cmath>
#include <iomanip>
#include <getopt.h>
#include <algorithm>

#include "dssm-player.hpp"

using std::cout;
using std::endl;

/**
 * DataReader
 * Constructors
 */

DataReader::DataReader(std::string src, std::string mIpAddr) : mLogSrc(src) {
  this->mLogFile.reset(new std::fstream());
  this->mLogFile->open(src, std::ios::in | std::ios::binary);
  if (mIpAddr == "empty") {
    this->mIpAddr = nullptr;
  } else {
    this->mIpAddr.reset(new std::string(mIpAddr));
  }
}

// TODO xvalueかlvalueかどうかをランタイムで判断して処理を変えるコピー関数。
// copyとmoveダサすぎる。

/**
 * copy constrctor
 */
DataReader::DataReader(DataReader const& rhs) {
	this->mTime = rhs.mTime;
	this->mData = rhs.mData;
	this->mDataSize = rhs.mDataSize;
	this->mProperty = rhs.mProperty;
	this->mPropertySize = rhs.mPropertySize;
	this->mFullData = rhs.mFullData;
	this->mStreamId = rhs.mStreamId;
	this->mBufferNum = rhs.mBufferNum;
	this->mCycle = rhs.mCycle;
	this->mStartTime = rhs.mStartTime;
	this->writeCnt = rhs.writeCnt;
	this->mLogSrc = rhs.mLogSrc;
	this->fulldata = fulldata;

	// SSMApiとPConnectorにはコピーコンストラクタがない。
	// this->ssmApi = ssmApi;
	// this->con = std::move(con);*/
	this->mStreamName = rhs.mStreamName;
	this->mStartPos = rhs.mStartPos;
	this->mEndPos = rhs.mEndPos;
	this->mPropertyPos = rhs.mPropertyPos;
	this->mLogFile.reset(new std::fstream());
	this->mLogFile->open(mLogSrc, std::ios::in | std::ios::binary);
	// rhsの現在を位置をゲット。
	auto pos = rhs.mLogFile->tellg();
	// rhsと同じ位置までファイルの位置をシークする。
	this->mLogFile->seekg(pos);
	if (rhs.mIpAddr == nullptr) {
		this->mIpAddr = nullptr;
	} else {
		this->mIpAddr.reset(new std::string(*rhs.mIpAddr));
	}
}

/**
 * move constructor
 */
DataReader::DataReader(DataReader&& rhs) noexcept {
	this->mTime = rhs.mTime;
	this->mData = rhs.mData;
	this->mDataSize = rhs.mDataSize;
	this->mProperty = rhs.mProperty;
	this->mPropertySize = rhs.mPropertySize;
	this->mFullData = rhs.mFullData;
	this->mStreamId = rhs.mStreamId;
	this->mBufferNum = rhs.mBufferNum;
	this->mCycle = rhs.mCycle;
	this->mStartTime = rhs.mStartTime;
	this->writeCnt = rhs.writeCnt;
  this->mLogFile = std::move(rhs.mLogFile);
  this->mIpAddr = std::move(rhs.mIpAddr);
	this->mLogSrc = std::move(rhs.mLogSrc);
	this->ssmApi = std::move(rhs.ssmApi);
	this->con = std::move(rhs.con);
	this->mStreamName = std::move(rhs.mStreamName);
	this->mStartPos = rhs.mStartPos;
	this->mEndPos = rhs.mEndPos;
	this->mPropertyPos = rhs.mPropertyPos;
	this->p = std::move(p);
	
	this->fulldata = fulldata;
	
  rhs.mLogFile = nullptr;
  rhs.mIpAddr = nullptr;
}

/**
 * Destructor
 */
DataReader::~DataReader() {
}

/* Constructor Fin */

void DataReader::init() {
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
	fulldata = nullptr;
	cout << std::setprecision(15);
}

bool DataReader::getLogInfo() {
  std::string line;
  mLogFile->seekg(0, std::ios::beg);
	if(!getline(*mLogFile, line))
		return false;
	std::istringstream data(line.c_str());

	data
		>> mStreamName
		>> mStreamId
		>> mDataSize
		>> mBufferNum
		>> mCycle
		>> mStartTime
		>> mPropertySize
		;
	if(data.fail())
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
	// cout << mPropertySize << " " << mDataSize << endl;
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

bool DataReader::readProperty() {
	int curPos;
	curPos = mLogFile->tellg();
	mLogFile->seekg(mPropertyPos, std::ios::beg);
	mLogFile->read((char *)mProperty, mPropertySize);
	mLogFile->seekg(curPos, std::ios::beg);
	return true;
}

bool DataReader::read() {
	mLogFile->read((char *)&mTime, sizeof(ssmTimeT));
	mLogFile->read(mData, mDataSize);
	return mLogFile->good();
}

bool SSMLogParser::optAnalyze(int aArgc, char **aArgv) {
	if (aArgc < 2) {
		cout << "USAGE dssm-player <log file1> <log file2:mIpAddr> ..." << endl;
		return false;
	}

	for (int i = 1; i < aArgc; ++i) {
		this->streamArray.emplace_back(aArgv[i]);
	}

	return true;
}

/**
 * Constructors
 */

SSMLogParser::SSMLogParser() {
}
SSMLogParser::~SSMLogParser() {
}

bool SSMLogParser::open() {

	// ログファイルを開く。mIpAddrがあれば設定、なければnullptrを設定。
	for (auto &&args : this->streamArray) {
		// 引数がログファイル：IPアドレスの形を取る場合
		if (args.find_first_of(':') != std::string::npos) {
			// ':' が来る最初の場所を取得してsplitする。
			auto colom = args.find_first_of(':');
			DataReader dataReader(args.substr(0, colom), args.substr(colom + 1));
			if (!dataReader.mLogFile->is_open()) {
				fprintf(stderr, "Error SSMLogParser::open  Log file cannot open.\n");
				return false;
			}
			
			// ストリーム名やcycle, データサイズなどの基本情報をメンバに保存
			dataReader.getLogInfo();
			
			// mLogFileに保存
			this->mLogFile.emplace_back(dataReader);
		} else {
			DataReader dataReader(args);
			if (!dataReader.mLogFile->is_open()) {
				fprintf(stderr, "Error SSMLogParser::open  Log file cannot open.\n");
				return false;
			}
			dataReader.getLogInfo();
			this->mLogFile.emplace_back(dataReader);
		}
	}

	return true;
}

bool SSMLogParser::create() {
	for (auto &&log : mLogFile) {
		cout << log.mStreamName << " create" << endl;
		if (log.mIpAddr == nullptr) {
			// SSMApi
			log.ssmApi.reset(new SSMApiBase(log.mStreamName.c_str(), log.mStreamId));

			// データが読み出されるポインタをssmapiにset
			log.ssmApi->setBuffer(
				log.mData,
				log.mDataSize,
				log.mProperty,
				log.mPropertySize
			);
			log.ssmApi->create(calcSSM_life(log.mBufferNum, log.mCycle), log.mCycle);
		} else {
			// PConnector
			log.con.reset(new PConnector(log.mStreamName.c_str(), log.mStreamId));

			// IPAddrをセット
			log.con->setIpAddress(log.mIpAddr->c_str());

			// PConnector用のfulldata
			log.fulldata = new char[log.mDataSize + sizeof(ssmTimeT)];

			// fulldata+8をmDataにセット(先頭8byteは時刻データ)
			log.mData = &(log.fulldata[8]);

			log.con->setBuffer(
				log.mData,
				log.mDataSize,
				log.mProperty,
				log.mPropertySize,
				log.fulldata
			);
			log.con->create(calcSSM_life(log.mBufferNum, log.mCycle), log.mCycle);
			log.con->createDataCon();
		}
	}
	cout << "end create stream" << endl;
	return true;
}

int main(int argc, char* argv[]) {
	SSMLogParser parser;
	parser.optAnalyze(argc, argv); // ログファイル名取得
	parser.open(); // ログファイル展開
	
	// SSMの初期化
	if (!initSSM()) {
		fprintf(stderr, "Error main cannot init SSM.\n");
		return 0;
	}
	
	parser.create();
}