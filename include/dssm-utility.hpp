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

		/**
		 * @brief タプルを関数の引数に展開し, std::__invokeを呼び出す。
		 */
		template <class Fn, class Tuple, size_t... _Idx>
		constexpr decltype(auto)
		apply_impl(Fn&& f, Tuple&& t, std::index_sequence<_Idx...>) {
			return std::__invoke(
				std::forward<Fn>(f),
				std::get<_Idx>(std::forward<Tuple>(t))...
			);
		}

		/**
		 * @brief apply(f, t)でfにtupleを展開し実行する。apply_implへのアダプタ。c++14で動作可能。
		 */
		template <class Fn, class Tuple>
		constexpr decltype(auto)  // この時点で戻り値はわからない
		apply(Fn&& f, Tuple&& t) {
			using Indices 
				= std::make_index_sequence<
					std::tuple_size<std::remove_reference_t<Tuple>>::value
				>;
			return apply_impl(
				std::forward<Fn>(f),
				std::forward<Tuple>(t),
				Indices{}
			);
		}

	} // namespace util
} // namespace dssm

#endif // _INC_DSSM_UTILITY_