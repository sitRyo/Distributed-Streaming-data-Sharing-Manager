/**
 * utility.hpp
 * 2019/12/19 R.Gunji
 * 複数のインスタンスで使用するAPI群
 */

#ifndef _INC_DSSM_UTILITY_
#define _INC_DSSM_UTILITY_

#include <libssm.h>

#include <iostream>
#include <type_traits>
#include <utility>

namespace dssm {
	namespace util {

		/**
		 * 16byteずつhexdumpする。
		 */
		void hexdump(char *p, uint32_t len);
		
		/**
		 * 送信時のパケットの大きさを決めるためにインスタンス生成時に呼ばれる。
		 * thrd_msgの構造が変化したらここを変更すること。
		 */
		uint32_t countThrdMsgLength();

		/**
		 * 送信時のパケットの大きさを決めるためにインスタンス生成時に呼ばれる。
		 * ssm_msgの構造が変化したらここを変更すること。
		 */
		uint32_t countDssmMsgLength();

	} // namespace util
} // namespace dssm

#endif // _INC_DSSM_UTILITY_