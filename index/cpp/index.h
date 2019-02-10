#pragma once
#include <unordered_map>
#include <base/base.h>
#include <cppjieba/Jieba.hpp>
#include "../../common/util.hpp"
#include "index.pb.h"

namespace doc_index {

typedef doc_index_proto::DocInfo DocInfo;
typedef doc_index_proto::KwdInfo KwdInfo;
typedef doc_index_proto::Weight Weight;
// 正排索引(存放在一个vector)
typedef std::vector<DocInfo> ForwardIndex;
// 倒排拉链(存放在一个vector)
typedef std::vector<Weight> InvertedList;
// 倒排索引(一个关键字对于一个倒排拉链,采用Map)
typedef std::unordered_map<std::string, InvertedList> InvertedIndex;

struct WordCnt {
  int32_t title_cnt;
  int32_t content_cnt;
  int32_t first_pos;  
  //为了后面方便生成摘要,该词在正文中第一次出现的位置
  WordCnt() 
    : title_cnt(0)
    , content_cnt(0)
    , first_pos(-1) 
  {}
};

typedef std::unordered_map<std::string, WordCnt> WordCntMap;

// Index包含了实现索引的数据结构以及索引需要提供的API接口
class Index {
public:
  // 1.索引在现实意义中,其实只存在一份,不用存在多份,可以采用单例模式实现
  // 2.凡是需要使用索引对象, 就必须通过Instance这个静态接口来访问索引, 而不允许使用Index类创建其他的对象
  // 3.本模块采用懒汉模式实现
  static Index* Instance() {
    if (inst_ == NULL) {
      inst_ = new Index();
    }
    return inst_;
  }

  Index();

  // 制作索引:读取raw_input文件,分析生成内存中的索引结构
  bool Build(const std::string& input_path);
  // 保存索引:基于protobuf把内存中的索引结构写到文件中
  bool Save(const std::string& output_path);
  // 加载索引:把文件中的索引结构加载到内存中
  bool Load(const std::string& index_path);
  // 反解索引:内存中的索引结构按照格式化打印到文件中
  bool Dump(const std::string& forward_dump_path, 
            const std::string& inverted_dump_path);
  // 查正排:给定文档id,获取到文档详细内容(取vector的下标)
  const DocInfo* GetDocInfo(uint64_t doc_id) const;
  // 查倒排:给定关键词, 获取到倒排拉链
  const InvertedList* GetInvertedList(const std::string& key) const;
  // 分词时过滤掉暂停词
  void CutWordWithoutStopWord(const std::string& query, std::vector<std::string>* words);
  // sort比较函数作为public, 供后面server模块也去使用
  static bool CmpWeight(const Weight& w1, const Weight& w2);
private:
  // 采用组合关系降低耦合度
  ForwardIndex forward_index_;     //正排索引对象
  InvertedIndex inverted_index_;   //倒排索引对象
  cppjieba::Jieba jieba_;                   
  common::DictUtil stop_word_dict_;//暂停词字典
  static Index* inst_;             //单例对象

  // 以下函数为辅助构造索引的函数声明为私有
  const DocInfo* BuildForward(const std::string& line);
  void BuildInverted(const DocInfo& doc_info);
  void SortInverted();
  void SplitTitle(const std::string& title, DocInfo* doc_info);
  void SplitContent(const std::string& content,
                    DocInfo* doc_info);
  int32_t CalcWeight(const WordCnt& word_cnt);
  bool ConvertToProto(std::string* proto_data);
  bool ConvertFromProto(const std::string& proto_data);
};

}  
