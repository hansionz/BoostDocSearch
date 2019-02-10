#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <unordered_set>
#include <boost/algorithm/string.hpp>
#include <sys/time.h>

namespace common {

// 这个类包含了所有和字符串相关的操作(util工具)
class StringUtil {
public:
  //压缩关闭
  // 例如, 如果待切分字符串:
  // aaa\3\3bbb
  // 如果是 compress_off, 就是切分成三个部分
  // 如果是 compress_on, 就是切分成两个部分
  // (相邻的 \3 被压缩成一个)
  // vim中输入不可见字符
  //   ctrl+v + 3 + ' ' +删除空格
  static void Split(const std::string& line,
      std::vector<std::string>* tokens,
      const std::string& split_char) {
    boost::split(*tokens, line,
        boost::is_any_of(split_char),//字符串中的任意一个字符都可以作为分割符
        boost::token_compress_off);
  }
};

// 处理字典的工具类
class DictUtil {
public:
  // 将暂停词表加载到内存中
  bool Load(const std::string& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
      return false;
    }
    std::string line;
    while (std::getline(file, line)) {
      set_.insert(line);
    }
    file.close();
    return true;
  }

  // 查找暂停词表中是否存在
  bool Find(const std::string& key) const {
    // set.find返回值迭代器，找到不能等于end,没找到等于end
    return set_.find(key) != set_.end();
  }

private:
  std::unordered_set<std::string> set_;
};

// 文件操作工具
class FileUtil {
public:
  static bool Read(const std::string& input_path,
      std::string* content) {
    std::ifstream file(input_path.c_str());
    if (!file.is_open()) {
      return false;
    }
    // 获取到文件的长度
    // 移动文件指针到末尾
    file.seekg(0, file.end);
    // [注意!!] 使用此方式来读文件, 文件最大不能超过2G
    // 获取文件指针相对于开始的偏移量,就是文件的长度
    int length = file.tellg();
    // 获取到长度后把文件指针移动到开始，否则会从末尾开始读
    file.seekg(0, file.beg);

    // 将缓冲区的长度置位初始值
    // 不用加1，不是C风格的字符串
    content->resize(length);
    file.read(const_cast<char*>(content->data()), length);
    file.close();
    return true;
  }

  static bool Write(const std::string& output_path,
      const std::string& content) {
    std::ofstream file(output_path.c_str());
    if (!file.is_open()) {
      return false;
    }
    // std::string data相当于c_str
    file.write(content.data(), content.size());
    file.close();
    return true;
  }
};

// 获取时间戳
class TimeUtil {
public:
  // tv_sec 秒
  // tv_usec 毫秒
  
  // 获取到秒级时间戳
  static int64_t TimeStamp() {
    struct ::timeval tv;
    ::gettimeofday(&tv, NULL);
    return tv.tv_sec;
  }

  // 获取到毫秒级时间戳
  static int64_t TimeStampMS() {
    struct ::timeval tv;
    ::gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
  }

  // 获取到微秒级时间戳
  static int64_t TimeStampUS() {
    struct ::timeval tv;
    ::gettimeofday(&tv, NULL);
    // 1e6 = 10的6次方(科学计数法)
    return tv.tv_sec * 1e6 + tv.tv_usec;
  }
};
}  // end common
