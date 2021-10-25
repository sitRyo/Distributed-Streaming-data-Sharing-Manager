/**
 * dssm-utility.cpp
 * 2020/5/29 R.Gunji
 * dssm-utility.hppの実装
 */

#include "dssm-utility.hpp"

namespace dssm {
	namespace util {

		/**
		 * 16byteずつhexdumpする。
		 */
		void hexdump(char *p, uint32_t len) {
			for (auto i = 0; i < len; ++i) {
				if (i % 16 == 0 and i != 0) std::printf("\n");
				std::printf("%02X ", p[i] & 0xff);
			}
			std::printf("\n");
		}

		/**
		 * 送信時のパケットの大きさを決めるためにインスタンス生成時に呼ばれる。
		 * thrd_msgの構造が変化したらここを変更すること。
		 */
		uint32_t countThrdMsgLength() {
			thrd_msg tmsg;
			uint32_t len = 0;
			len += sizeof(tmsg.msg_type);
			len += sizeof(tmsg.res_type);
			len += sizeof(tmsg.tid);
			len += sizeof(tmsg.time);
			
			return len;
		}

		/**
		 * 送信時のパケットの大きさを決めるためにインスタンス生成時に呼ばれる。
		 * ssm_msgの構造が変化したらここを変更すること。
		 */
		uint32_t countDssmMsgLength() {
			ssm_msg msg;
			uint32_t len = 0;
			len += sizeof(msg.msg_type);
			len += sizeof(msg.res_type);
			len += sizeof(msg.cmd_type);
			len += sizeof(msg.name);
			len += sizeof(msg.suid);
			len += sizeof(msg.ssize);
			len += sizeof(msg.hsize);
			len += sizeof(msg.time);
			len += sizeof(msg.saveTime);

			return len;
		}



	} // namespace util
} // namespace dssm
