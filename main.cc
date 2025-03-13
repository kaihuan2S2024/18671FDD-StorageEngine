#include <iostream>
#include "sql_rc.h"

int main() {
  std::cout << "Hello, World!" << std::endl;

  // Test that result_code can be used in main.cc
  ResultCode code = ResultCode::kOk;
  std::string str_code = ToString(code);
  std::cout << str_code << std::endl;
  return 0;
}
