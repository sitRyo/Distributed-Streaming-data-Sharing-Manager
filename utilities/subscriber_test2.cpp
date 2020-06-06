/**
 * subscriber_test2.cpp
 * 2つのSSMSubscriberを使ったサンプルコード
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

  // SSMApiの情報をここに入力する。
  // StreamName, StreamId, dataのサイズ, propertyのサイズ
  streams.push_back({SNAME_INT, 0, sizeof(int), 0});
  streams.push_back({SNAME_INT, 1, sizeof(int), 0});

  // ストリームを追加。
  sub.add_stream(streams);

  // ストリームをオープン
  sub.stream_open();
  
  // Subscribeするストリーム情報を設定する。
  // 最新のデータ取得
  // StreamName, StreamId, dataのサイズ, propertyのサイズ, 条件の指定(最新のデータを取得 = readLast)
  SubscriberSet ss({SNAME_INT, 0, sizeof(int), 0}, OBSV_COND_LATEST, OBSV_COND_TRIGGER);
  
  // 時間指定
  // StreamName, StreamId, dataのサイズ, propertyのサイズ,
  // 条件の指定(時刻指定でデータを取得 = readTime), 同期するストリーム(ここでは intSsm1)
  SubscriberSet ss2({SNAME_INT, 1, sizeof(int), 0}, OBSV_COND_TIME, OBSV_COND_NO_TRIGGER, {SNAME_INT, 0});
  ssm_api_pair_map api_pair_map;
  
  subscriber_set.emplace_back(ss);
  subscriber_set.emplace_back(ss2);

  // ローカル変数を条件としてキャプチャすることもできる。
  bool flag = true;
  
  // ローカル条件
  auto cond = [&flag]() -> bool {
    return flag;
  };

  // コールバック
  auto callback = [](intSsm_k data1, intSsm_k data2) {
    printf("data: %d\n", data1.num + data2.num);
  };

  // ローカル条件は bool()
  std::function<bool()> local_cond = cond;
  // コールバックはvoid(Args...)
  std::function<void(intSsm_k, intSsm_k)> print = callback;

  // subscriberを登録。
  // 使用したいsubscriberごとにinvokeする。
  sub.register_subscriber(subscriber_set, local_cond, print, api_pair_map);

  // subscriberを開始する。
  sub.start();
}