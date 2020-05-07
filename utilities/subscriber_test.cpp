#include "ssm-subscriber.hpp"
#include <iostream>

int main() {
  SSMSubscriber sub;
  sub.init_subscriber();
  std::cout << "recv successed\n";
}