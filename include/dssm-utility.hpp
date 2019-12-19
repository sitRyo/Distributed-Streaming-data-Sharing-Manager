/**
 * utility.hpp
 * 2019/12/19 R.Gunji
 * 複数のインスタンスで使用するAPI群
 */

#ifndef _INC_DSSM_UTILITY_
#define _INC_DSSM_UTILITY_

#include <iostream>
#include <libssm.h>

namespace dssm {
	namespace util {

		/**
		 * 16byteずつhexdumpする。
		 */
		void hexdump(char *p, uint32_t len) {
			for (int i = 0; i < len; ++i) {
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
			len += sizeof(msg.ssize);
			len += sizeof(msg.ssize);
			len += sizeof(msg.hsize);
			len += sizeof(msg.time);
			len += sizeof(msg.saveTime);

			return len;
		}

	} // namespace util
} // namespace dssm

#endif // _INC_DSSM_UTILITY_