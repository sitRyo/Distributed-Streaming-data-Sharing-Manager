#include "ssm-subscriber.hpp"
#include "intSsm.h"
#include <iostream>

// using ssm_pair = typename SSMSubscriber::ssm_api_pair;

int main() {
  SSMSubscriber sub;
  sub.init_subscriber();

  std::vector<Stream> subscribers;
  subscribers.push_back({SNAME_INT, 0, sizeof(int), 0});
  sub.add_subscriber(subscribers);
  sub.start();

  int cnt = 0;
  while (true) {
    sub.access_subscriber({SNAME_INT, 0});
    sleep(1);
  }

  std::cout << "recv successed\n";
}