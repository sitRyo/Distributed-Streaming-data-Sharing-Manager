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

DataReader::DataReader(std::string src, DataReaderMode mode, std::string mIpAddr) : mLogSrc(src), mDataReaderMode(mode) {
  this->mLogFile.reset(new std::fstream());
  this->mLogFile->open(src, std::ios::in | std::ios::binary);
	this->mOutFile.reset(new std::fstream());
	this->mOutFile->open(src + ".out", std::ios::out);
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
	this->fulldata = rhs.fulldata;
	this->mDataReaderMode = rhs.mDataReaderMode;
	this->mCurrentTime = rhs.mCurrentTime;
	this->mNextTime = rhs.mNextTime;
	this->tid = rhs.tid;

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

	this->mOutFile.reset(new std::fstream());
	this->mOutFile->open(mLogSrc + ".out", std::ios::out);

	// rhsの現在位置をゲット
	pos = rhs.mOutFile->tellg();
	this->mOutFile->seekg(pos);

	if (rhs.mDataReaderMode == SSMApiMode) {
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
	this->mOutFile = std::move(rhs.mOutFile);
  this->mIpAddr = std::move(rhs.mIpAddr);
	this->mLogSrc = std::move(rhs.mLogSrc);
	this->ssmApi = std::move(rhs.ssmApi);
	this->con = std::move(rhs.con);
	this->mStreamName = std::move(rhs.mStreamName);
	this->mStartPos = rhs.mStartPos;
	this->mEndPos = rhs.mEndPos;
	this->mPropertyPos = rhs.mPropertyPos;
	this->p = std::move(p);
	this->mDataReaderMode = rhs.mDataReaderMode;
	this->mCurrentTime = rhs.mCurrentTime;
	this->mNextTime = rhs.mNextTime;
	this->tid = rhs.tid;
	this->fulldata = rhs.fulldata;
	
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
	mDataReaderMode = Init;
	mCurrentTime = 0.0;
	mNextTime = 0.0;
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
#if 1
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
	mNextTime = gettimeSSM_real() + readNextTimeNotSeek() - mStartTime;
	tid = 0;

	// アウトファイル書き込み
	this->writeStreamInfo();
	return true;
}

void DataReader::writeStreamInfo() {
	*mOutFile 
		<< "Stream Name: " << mStreamName << "\n"
		<< "Stream Id: " << mStreamId << "\n"
		<< "DataSize: " << mDataSize << "\n"
		<< "BufferNum: " << mBufferNum << "\n"
		<< "Cycle: " << mCycle << "\n"
		<< "Start Time: " << mStartTime << "\n"
		<< "Property Size: " << mPropertySize << "\n\n"
		<< "----start----\n\n"
		<< std::endl; // flush
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

ssmTimeT DataReader::readNextTimeNotSeek() {
	ssmTimeT time;
	mLogFile->read((char *)&time, sizeof(ssmTimeT));
	mLogFile->seekg(-sizeof(ssmTimeT), std::ios::cur);
	return time;
}

bool DataReader::writeOutFile() {
	*mOutFile << std::setprecision(18);	
  *mOutFile << "Time: " << mTime << "\n";
  for (auto i = 0UL; i < mDataSize; ++i) {
    if (i != 0 && i % 16 == 0) *mOutFile << "\n";
    // printf("%02X ", ((char *)mData)[i] & 0xff);
    *mOutFile << (((char *)mData)[i] & 0xff) << " ";
  }
  *mOutFile << "\n";
  return true;
}

bool DataReader::write(ssmTimeT currentTime) {
	if (currentTime >= mNextTime) {
		this->read();
		mNextTime = currentTime + this->readNextTimeNotSeek() - this->mTime;
		switch (this->mDataReaderMode) {
			case SSMApiMode:
				if (!ssmApi->write(currentTime)) {
					fprintf(stderr, "Error DataReader::write SSMApi cannot write.\n");
					return false;
				}

				// tidは個別に出力(保存場所が違うため)
				*mOutFile << "tid " << this->ssmApi->timeId << " ";
				break;
			case PConnectorMode:
				this->tid ++;
				if (!con->write(currentTime)) {
					fprintf(stderr, "Error DataReader::write SSMApi cannot write.\n");
					return false;
				}
				*mOutFile << "tid " << this->tid << " ";
				break;
			default:
				fprintf(stderr, "Error DataReader::write DataReader has Init mode.\n");
				return false;
		}

		// アウトファイルに共有メモリに書き込んだデータを記録
		this->writeOutFile();
	}

	return true;
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
 * SSMLogParser
 */

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
			DataReader dataReader(args.substr(0, colom), PConnectorMode ,args.substr(colom + 1));
			if (!dataReader.mLogFile->is_open()) {
				fprintf(stderr, "Error SSMLogParser::open  Log file cannot open.\n");
				return false;
			}

			if (!dataReader.mOutFile->is_open()) {
				fprintf(stderr, "Error SSMLogParser::open  Out file cannot open.\n");
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

			if (!dataReader.mOutFile->is_open()) {
				fprintf(stderr, "Error SSMLogParser::open  Out file cannot open.\n");
				return false;
			}

			dataReader.getLogInfo();
			this->mLogFile.emplace_back(dataReader);
		}
	}

	return true;
}

bool SSMLogParser::write() {
	auto currentTime = gettimeSSM_real();
	for (auto &&log : mLogFile) {
		if(!log.write(currentTime)) {
			return false;
		}
	}
	updateConsoleShow();
	return true;
}

void SSMLogParser::updateConsoleShow() {
	cout << std::setprecision(18);	
	cout << "\033[0;0H"; // カーソルを0行0列に変更する
	for (auto &&log : mLogFile) {
		cout << "\033[K"; // 右をクリア
		cout 
			<< log.mStreamName << "  " 
			<< ((log.mDataReaderMode == SSMApiMode) ? log.ssmApi->timeId : log.tid) << " "
			<< ((log.mDataReaderMode == SSMApiMode) ? log.ssmApi->time : log.con->time) << endl;
	}
}


bool SSMLogParser::create() {
	for (auto &&log : mLogFile) {
		cout << log.mStreamName << " create" << endl;
		switch (log.mDataReaderMode) {
			case SSMApiMode: 
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
				break;
			case PConnectorMode:
				// PConnector
				log.con.reset(new PConnector(log.mStreamName.c_str(), log.mStreamId, log.mIpAddr->c_str()));

				if (!log.con->initSSM()) {
					fprintf(stderr, "Error SSMLogParser::create PConnector cannot init SSM\n");
					log.con->terminate();
					return false;
				}

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

				if (!log.con->create(calcSSM_life(log.mBufferNum, log.mCycle), log.mCycle)) {
					fprintf(stderr, "Error SSMLogParser::create PConnector cannot create SSM\n");
					log.con->terminate();
					return false;
				}

				if (!log.con->createDataCon()) {
					fprintf(stderr, "Error SSMLogParser::create PConnector cannot createDataCon\n");
					log.con->terminate();
					return false;
				}
				break;
			default:
				fprintf(stderr, "Error SSMLogParser::create DataReader has Init mode\n");
				return false;
		}
	}
	cout << "end create stream" << endl;
	return true;
}

int SSMLogParser::commandAnalyze(char const *command) {
	std::istringstream data(command);
	std::string cmd;

	data >> cmd;
	if (!data.fail()) {
		if (cmd == "x" || cmd == "stop") {
			return 0;
		}

		if (cmd == "p" || cmd == "start") {
			return 1;
		}
	}

	return -1;
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

	cout << "\033[2J"; // 画面クリア
	cout << "\033[1;0H"; // カーソルを0行0列に変更する
	cout << "       stream       |    time id    |     time     |\n";
	// char command[256];
	bool isSleep = false;
	while (true) {
		if (!isSleep) {
			parser.write();
		}

		usleepSSM(1000); // 1msスリープ
		
		// コマンド解析
		// if (fgets(command, sizeof(command), stdin)) {
		// 	switch (parser.commandAnalyze(command)) {
		// 		case 0:
		// 			isSleep = true;
		// 			break;
		// 		case 1:
		// 			isSleep = false;
		// 			break;
		// 		default:
		// 			;// do nothing
		// 	}
		// }
	}

	cout << "end" << endl;
}