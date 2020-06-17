/**
 * subscriber_test.cpp
 * SSMSubscriberのサンプルコード
 */

#include "ssm-subscriber.hpp"
#include "intSsm.h"
#include <iostream>

using namespace ssm;

int main() {
  SSMSubscriber sub;

  // Subscriberの初期化
  sub.init_subscriber();

  // アクセスするストリーム情報
  std::vector<Stream> streams;

  // Subscriber情報
  std::vector<SubscriberSet> subscriber_set;

  Stream int_ssm1 {SNAME_INT, 0, sizeof(int), sizeof(intSsm_p)};
  Stream int_ssm2 {SNAME_INT, 1, sizeof(int), 0, "127.0.0.1"};

  // SSMApiの情報をここに入力する。
  // StreamName, StreamId, dataのサイズ, propertyのサイズ
  streams.push_back(int_ssm1);
  streams.push_back(int_ssm2);

  // ストリームを追加。
  sub.add_stream(streams);

  // ストリームをオープン
  sub.stream_open();
  
  // Subscribeするストリーム情報を設定する。
  // StreamName, StreamId, dataのサイズ, propertyのサイズ, 条件の指定, 同期の基準にするstreamの設定
  SubscriberSet ss {int_ssm1, OBSV_COND_LATEST, OBSV_COND_TRIGGER};
  SubscriberSet ss2 {int_ssm2, OBSV_COND_LATEST, OBSV_COND_NO_TRIGGER, {SNAME_INT, 0}};
  subscriber_set.emplace_back(ss);
  subscriber_set.emplace_back(ss2);
  
  ssm_api_pair_map pair_map;

  // ローカル変数を条件としてキャプチャすることもできる。
  bool flag = true;
  
  // ローカル条件
  std::function<bool()> local_cond = [&flag]() -> bool {
    return flag;
  };

  // コールバック
  std::function<void(intSsm_k, intSsm_p, intSsm_k)> callback = [&pair_map](intSsm_k data1, intSsm_p property1, intSsm_k data2) {
    const auto p_map1 = pair_map.at({SNAME_INT, 0});
    const auto p_map2 = pair_map.at({SNAME_INT, 1});
    printf("num1 %d num2 %f\n", property1.num1, property1.num2);
    printf("time %lf, tid %d\n", p_map1.time, p_map1.tid);
    printf("time %lf, tid %d\n", p_map2.time, p_map2.tid);
    printf("data: %d\n", data1.num + data2.num);
  };

  // subscriberを登録。
  // 使用したいsubscriberごとにinvokeする。
  sub.register_subscriber(subscriber_set, local_cond, callback, pair_map);

  // subscriberを開始する。
  sub.start();
}