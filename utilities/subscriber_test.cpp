#include "ssm-subscriber.hpp"
#include "intSsm.h"
#include <iostream>

// using ssm_pair = typename SSMSubscriber::ssm_api_pair;

int main() {
  SSMSubscriber sub;
  sub.init_subscriber();

  std::vector<Stream> subscribers;
  std::vector<SubscriberSet> subscriber_set;

  subscribers.push_back({SNAME_INT, 0, sizeof(int), 0});
  sub.add_stream(subscribers);

  sub.stream_open();
  
  SubscriberSet ss({SNAME_INT, 0, sizeof(int), 0}, OBSV_COND_LATEST);
  subscriber_set.emplace_back(ss);

  bool flag = true;
  
  auto cond = [&flag]() -> bool {
    return flag;
  };

  auto func = [](intSsm_k data) {
    printf("data: %d\n", data.num);
  };

  std::function<bool()> local_cond = cond;
  std::function<void(intSsm_k)> print = func;

  sub.register_subscriber<intSsm_k>(subscriber_set, local_cond, print);

  sub.start();
}