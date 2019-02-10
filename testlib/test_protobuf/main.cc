#include "hello.pb.h"

int main() {
  // 序列化
  // 按照二进制的方式进行序列化
  // 优点:效率比较高(压缩的效率高, 网络传输或者数据存储的时候资源开销就小)
  // 缺点:可读性很差, 不方便进行调试
  Hello hello;
  hello.set_name("zs");//命名和自己设置的向符合本
  hello.set_score(65);
  std::string buf;
  hello.SerializeToString(&buf);
  std::cout << buf << std::endl;

  // 2. 反序列化
  Hello hello_result;
  hello_result.ParseFromString(buf);
  std::cout << hello_result.name() << std::endl;
  std::cout << hello_result.score() << std::endl;
  return 0;
}
