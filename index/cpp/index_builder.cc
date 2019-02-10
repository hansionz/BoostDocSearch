// 索引制作程序.
// 读入 raw_input 文件
// 输出索引文件
#include <base/base.h> 
#include "index.h"

DEFINE_string(input_path, "../data/tmp/raw_input",
              "输入文件");
DEFINE_string(output_path, "../data/output/index_file",
              "输出文件");

int main(int argc, char* argv[]) {
  base::InitApp(argc, argv);

  // 获取索引单例
  doc_index::Index* index = doc_index::Index::Instance();
  CHECK(index->Build(fLS::FLAGS_input_path));
  CHECK(index->Save(fLS::FLAGS_output_path));
  return 0;
}
