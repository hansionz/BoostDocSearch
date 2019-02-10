#include <iostream>
#include <gflags/gflags.h>

DEFINE_string(ip, "127.0.0.1", "这个是ip地址");
DEFINE_int32(port, 80, "这个是端口号");
DEFINE_bool(use_tcp, true, "是否使用 TCP 协议");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::cout << FLAGS_ip << std::endl;
  std::cout << FLAGS_port << std::endl;
  std::cout << FLAGS_use_tcp << std::endl;
  return 0;
}
