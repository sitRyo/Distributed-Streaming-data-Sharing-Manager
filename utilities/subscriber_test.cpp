#include "ssm-subscriber.hpp"
#include <iostream>

int main() {
  SSMSubscriber sub;
  sub.init_subscriber();

  std::vector<std::string> name;
  name.push_back("urg_fs");
  name.push_back("dsm_gl");
  name.push_back("resampler");
  sub.add_subscriber(name);
  sub.send_subscriber();

  std::cout << "recv successed\n";
}