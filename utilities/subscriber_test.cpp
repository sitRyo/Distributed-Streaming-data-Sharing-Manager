#include "ssm-subscriber.hpp"
#include <iostream>

using ssm_pair = typename SSMSubscriber::ssm_api_pair;

int main() {
  SSMSubscriber sub;
  sub.init_subscriber();

  std::vector<ssm_pair> name;
  name.push_back({"urg_fs", 0});
  name.push_back({"dsm_gl", 0});
  name.push_back({"resampler", 0});
  sub.add_subscriber(name);
  sub.send_subscriber();

  std::cout << "recv successed\n";
}