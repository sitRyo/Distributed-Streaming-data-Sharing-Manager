#include "ssm-subscriber.hpp"
#include <iostream>

int main() {
  SSMSubscriber sub;
  sub.init_subscriber();
  printf("ok\n");
  std::cout << "recv successed\n";
}