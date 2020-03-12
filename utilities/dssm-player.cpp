/**
 * dssm-player
 */

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <cmath>
#include <iomanip>
#include <getopt.h>
#include <algorithm>
#include <numeric>
#include <thread>

#include "dssm-player.hpp"

using std::cout;
using std::endl;

/**
 * DataReader
 * Constructors
 */

DataReader::DataReader(std::string const& src, DataReaderMode const& mode, std::string mIpAddr) : mLogSrc(src), mDataReaderMode(mode) {
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

bool DataReader::getLogInfo() {
	writeCnt = 0;
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
		<< "\n******************** Stream *************************\n"
		<< "Stream Name: " << mStreamName << "\n"
		<< "Stream Id: " << mStreamId << "\n"
		<< "DataSize: " << mDataSize << "\n"
		<< "BufferNum: " << mBufferNum << "\n"
		<< "Cycle: " << mCycle << "\n"
		<< std::fixed << std::setprecision(12)
		<< "Start Time: " << mStartTime << "\n"
		<< "Property Size: " << mPropertySize
		<< "\n*****************************************************\n"
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
	tid = -1;

	// アウトファイル書き込み
	// this->writeStreamInfo();
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
	mLogFile->seekg(-8, std::ios::cur); // 時刻データ分巻き戻す
	return time;
}

/**
 * ログファイルをダンプして出力。
 * write -> writeOutFile
 */
bool DataReader::writeOutFile(ssmTimeT const& currentTime) {
	*mOutFile << std::setprecision(18);	
  // *mOutFile << "Time: " << currentTime << "\n\n" << std::hex;
	*mOutFile << currentTime << "\n" << std::hex;
  for (auto i = 0UL; i < mDataSize; ++i) {
    *mOutFile << (((char *)mData)[i] & 0xff) << " ";
  }
  // *mOutFile << "\n\n" << "----------------- \n" << endl;
	*mOutFile << endl;
  return true;
}

bool DataReader::write(ssmTimeT const& currentTime) {
	if (currentTime >= mNextTime) {
		writeCnt++;
		this->read();
		mNextTime = currentTime + this->readNextTimeNotSeek() - this->mTime;
		switch (this->mDataReaderMode) {
			case SSMApiMode:
				if (!ssmApi->write(currentTime)) {
					fprintf(stderr, "Error DataReader::write SSMApi cannot write.\n");
					return false;
				}

#ifdef DEBUG
				// tidは個別に出力(保存場所が違うため)
				*mOutFile << std::dec << this->ssmApi->timeId << " ";
#endif
				break;
			case PConnectorMode:
				this->tid ++;
				if (!con->write(currentTime)) {
					fprintf(stderr, "Error DataReader::write SSMApi cannot write.\n");
					return false;
				}

#ifdef DEBUG
				// *mOutFile << std::dec << this->tid << " ";
#endif
				break;
			default:
				fprintf(stderr, "Error DataReader::write DataReader has Init mode.\n");
				return false;
		}

#ifdef DEBUG
		// アウトファイルに共有メモリに書き込んだデータを記録
		this->writeOutFile(currentTime);
#endif

		if (writeCnt > 1000) {
			exit(1);
		}
	}

	return true;
}

/**
 * ログファイルをhexdumpして出力する。
 * @required from optAnalyze
 */
void SSMLogParser::hexdumpLogFile() {
	for (auto&& it : this->mLogFile) {
		// eofになるまでダンプし続ける。
		while (it.read()) {
			it.writeOutFile(it.mTime);
		}
	}
}

/**
 * オプションでhelpが指定されたときに表示される文字列を表示する関数。
 * @required from optAnalyze
 */
void SSMLogParser::printHelp() {
	std::cout << "HELP\n"
					  << "\t-n | --network : ログファイルのオフセットを計算し, 指定したIPアドレスを持つPCでログファイルを再生する.\n"
						<< "\t-i | --ignore  : 指定したIPアドレスを持つPCでログファイルを再生する.\n"
						<< "\t-h | --help    : helpを表示する。\n"
						<< "ex) dssm-player -n a.log b.log:172.168.1.230\n"
						<< std::flush;
}


bool SSMLogParser::optAnalyze(int aArgc, char **aArgv, char& option) {
	std::string playerMode = "";
	int opt, optIndex = 0;
	struct option longOpt[] = {
		{"network", no_argument, 0, 'n'},
		{"ignore", no_argument, 0, 'i'},
		{"dump", no_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'}, 
	};

	// 選べるモードは1つだけ。もし2つ以上指定されていたらエラーを返す。
	// dumpだけは一緒に選べてもいいかもしれないね.(もしそうするならbitで管理すればいい)
	int option_cnt = 0;
	while ((opt = getopt_long( aArgc, aArgv, "nidh", longOpt, &optIndex)) != -1) {
		switch (opt) {
			case 'n': 
				option_cnt ++;
				option = 'n';
				break;
			case 'i':
				option_cnt ++;
				option = 'i';
				break;
			case 'd':
				option_cnt ++;
				option = 'd';
				break;
			case 'h': 
				printHelp();
				break;
			default: 
				; // do nothing
		}
	}

	if (aArgc < 2) {
		printHelp();
		return false;
	}

	if (option_cnt >= 2) {
		cout << "指定できるオプションは1つだけです.\n";
		printHelp();
		return false;
	}

	for (int i = optind; i < aArgc; ++i) {
		this->streamArray.emplace_back(aArgv[i]);
		cout << aArgv[i] << endl;
	}

	return true;
}

/**
 * SSMLogParser
 */

/**
 * Constructors
 */

SSMLogParser::SSMLogParser() {}
SSMLogParser::~SSMLogParser() {}

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

	// 大したコストではないので、networkモードで使うオフセットを計算しておく。
	this->calculateOffset();

	return true;
}

bool SSMLogParser::write() {
	// PCの現在時刻を格納する.
	ssmTimeT currentTime = gettimeSSM_real();
	// この関数が呼ばれたときにだけ初期化される.
	static ssmTimeT firstTime = currentTime;
	// 経過時間
	double elapsedTime = currentTime - firstTime;
	// 書き込みが終了したか否かを格納するフラグ.
	bool isWriteFinish = true;
	for (auto &&log : mLogFile) {
		// 書き込みに失敗した, もしくは経過時間がオフセットを超えていなかったら書き込みをしない.
		if(!log.write(currentTime) && log.mOffset > elapsedTime) {
			// オフセットを超えていない状態ならフラグは更新しない.
			isWriteFinish = log.mOffset > elapsedTime;
			continue;
		}
	}

#ifdef DEBUG
	updateConsoleShow();
#endif

	// ログを全て読み終えた = false
	return isWriteFinish;
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

/**
 * 再生したいログファイルの中で一番先に取得されたファイルのうち
 */
void SSMLogParser::calculateOffset() {
	ssmTimeT earliestTime = std::numeric_limits<double>::max();

	// 一番早い時間に取られたタイムスタンプを記録。
	for (auto&& log : this->mLogFile) 
		earliestTime = std::min(earliestTime, log.mStartTime);

	// その時刻との差を取る。
	for (auto& log : this->mLogFile) {
		log.mOffset = log.mStartTime - earliestTime;
		cout << log.mOffset << endl; // debug
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

bool SSMLogParser::play() {
	// SSMに接続
	if (!initSSM()) {
		fprintf(stderr, "Error main cannot init SSM.\n");
		return 0;
	}
	// 対象のPCにストリーム作成
	this->create();
	// option用に作った変数. 今は使ってない.
	char t;
	cout << "press ENTER to start." << endl;
	// こっちも使ってない.
	std::cin.get(t);

	cout << "\033[2J"; // 画面クリア
	cout << "\033[1;0H"; // カーソルを1行0列に変更する
	// cout << "       stream       |    time id    |     time     |\n"; // ssm-monitorを模倣
			
	// SSMの共有メモリにログデータを書き込む。
	// もし、ログファイルの書き込みが終わったらfalseを返してループを抜ける
	// 1ms止める
	while (this->write()) { /*usleep(1000);*/ }
	
	return true;
}

/**
 * SSMLogParser deleteOffset
 * Offsetをゼロにする。
 * この実装汚いかなぁ...?
 */
void SSMLogParser::deleteOffset() {
	for (auto& log : mLogFile) {
		log.mOffset = 0.0;
	}
}

int main(int argc, char* argv[]) {
	SSMLogParser parser;
	char option;

	// オプション解析
	if (!parser.optAnalyze(argc, argv, option)) {
		exit(0);
	}

	parser.open(); // ログファイル展開
	
	switch (option) {
		// network
		case 'n': 
			parser.play();
			break;
		
		// ignore
		case 'i':
			parser.deleteOffset();
			parser.play();
			break;
		
		// dump
		case 'd':
			// 与えられたログファイルをhexdumpして出力する。
			parser.hexdumpLogFile();
			break;
		// --help 
		default: 
				; // do nothing (いまのところ)
	}

	cout << "end" << endl;
}