#include "index.h"
#include <fstream>

// 使用gflags库对路径参数进行灵活控制
DEFINE_string(dict_path, "../../third_part/data/jieba_dict/jieba.dict.utf8", "词典路径");
DEFINE_string(hmm_path, "../../third_part/data/jieba_dict/hmm_model.utf8", "hmm词典路径");
DEFINE_string(user_dict_path, "../../third_part/data/jieba_dict/user.dict.utf8", "用户词典路径");
DEFINE_string(idf_path, "../../third_part/data/jieba_dict/idf.utf8", "idf词典路径");
DEFINE_string(stop_word_path, "../../third_part/data/jieba_dict/stop_words.utf8", "暂停词词典路径");

namespace doc_index {

// 静态变量必须在类外初始化,编译期间分配内存
Index* Index::inst_ = NULL;

Index::Index() : jieba_(FLAGS_dict_path,
    fLS::FLAGS_hmm_path,
    fLS::FLAGS_user_dict_path,
    fLS::FLAGS_idf_path,
    fLS::FLAGS_stop_word_path) {
    CHECK(stop_word_dict_.Load(fLS::FLAGS_stop_word_path));
}

bool Index::Build(const std::string& input_path) {
  LOG(INFO) << "Index Build";
  // 1. 打开文件并且按行读取文件, 针对每行进行解析
  //    打开的文件其实是 raw_input 文件. 这个文件是预处理之后得到的行文本文件. 每行对应一个 html
  //    每行又是一个三元组(jump_url, title, content)
  std::ifstream file(input_path.c_str());
  CHECK(file.is_open());
  std::string line;
  while (std::getline(file, line)) {
    // 2. 针对当前行, 更新正排索引.
    //    构造一个 DocInfo 对象, 把这个对象 push_back 到
    //    forward_index_ 这个 vector 中
    const DocInfo* doc_info = BuildForward(line);
    CHECK(doc_info != NULL);
    // 3. 根据当前的 DocInfo 更新倒排索引
    BuildInverted(*doc_info);
  }
  // 4. 把所有的 html 正排和倒排都更新完了之后, 针对所有的倒排
  //    拉链进行按照权重排序
  SortInverted();
  file.close();
  LOG(INFO) << "Index Build Done";
  return true;
}

const DocInfo* Index::BuildForward(const std::string& line) {
  // 1. 先对参数进行字符串切分, 切分成三个部分
  // strtok线程不安全、破坏原字符串 strtok_r线程安全
  std::vector<std::string> tokens;
  common::StringUtil::Split(line, &tokens, "\3");
  if (tokens.size() != 3) {
    LOG(FATAL) << "line split not 3 tokens! tokens.size()="
               << tokens.size();
    return NULL;
  }
  // 2. 构造一个 DocInfo 对象, 把切分结果先设置到对象中
  DocInfo doc_info;
  doc_info.set_doc_id(forward_index_.size());
  doc_info.set_title(tokens[1]);
  doc_info.set_content(tokens[2]);
  doc_info.set_jump_url(tokens[0]);
  // 暂时先把 show_url 设置成和 jump_url 一样
  // 实际上真是的搜索引擎中show_url只包含jump_url的域名部分
  doc_info.set_show_url(tokens[0]);
  // 3. 对标题和正文进行分词, 把分词结果也保存在 DocInfo 对象里
  SplitTitle(tokens[1], &doc_info);
  SplitContent(tokens[2], &doc_info);
  // 4. DocInfo 对象插入到 forward_index_ 中
  forward_index_.push_back(doc_info);
  return &forward_index_.back();
}

void Index::SplitTitle(const std::string& title,
    DocInfo* doc_info) {
  // 1. 调用 jieba 分词接口完成对标题的分词(offset 风格的接口)
  std::vector<cppjieba::Word> words;
  std::cout << title << std::endl;
  jieba_.CutForSearch(title, words); 
  //if(words.size() <= 1){
  //  LOG(FATAL) << "SplitTitle fail!";
  //  return;
  //}
  // 2. 把 jieba 分词结果风格转成我们需要的前闭后开区间的风格
  for (size_t i = 0; i < words.size(); ++i) {
    auto* token = doc_info->add_title_token();
    token->set_beg(words[i].offset);
    if (i + 1 < words.size()) {
      // i + 1 没有越界
      token->set_end(words[i + 1].offset);
    } else {
      token->set_end(title.size());
    }
  }
  return;
}

void Index::SplitContent(const std::string& content,
    DocInfo* doc_info) {
  // 1. 调用 jieba 分词接口完成对标题的分词(offset 风格的接口)
  std::vector<cppjieba::Word> words;
  jieba_.CutForSearch(content, words);
  // 2. 把 jieba 分词结果风格转成我们需要的前闭后开区间的风格
  for (size_t i = 0; i < words.size(); ++i) {
    auto* token = doc_info->add_content_token();
    token->set_beg(words[i].offset);
    if (i + 1 < words.size()) {
      // i + 1 没有越界
      token->set_end(words[i + 1].offset);
    } else {
      token->set_end(content.size());
    }
  }
  return;
}

void Index::BuildInverted(const DocInfo& doc_info) {
  // 构造倒排索引的核心流程
  // 1. 先统计好 doc_info 中每个词在标题和正文中出现的次数
  //    此时我们得到了一个hash表. key, 当前的关键词. value, 
  //    就是一个结构体, 结构体包含着该词在标题中出现的次数
  //    和该词在正文中出现的次数.
  WordCntMap word_cnt_map;
  for (int i = 0; i < doc_info.title_token_size(); ++i) {
    const auto& token = doc_info.title_token(i);
    std::string word = doc_info.title().substr(
          token.beg(),
          token.end() - token.beg());
    // 此处需要忽略词的大小写再进行统计.
    // Hello 和 hello 算作一个词出现两次
    boost::to_lower(word);
    // 此处需要忽略掉分词结果中的暂停词
    // 暂停词:了、于、标点符号等
    if (stop_word_dict_.Find(word)) {
      continue;
    }
    // 不存在则插入，存在则++cnt
    ++word_cnt_map[word].title_cnt;
  }
  // 计算每个词在正文中出现的次数
  for (int i = 0; i < doc_info.content_token_size(); ++i) {
    const auto& token = doc_info.content_token(i);
    std::string word = doc_info.content().substr(
          token.beg(),
          token.end() - token.beg());
    // 此处需要忽略词的大小写再进行统计.
    // Hello 和 hello 算作一个词出现两次
    boost::to_lower(word);
    // 此处需要忽略掉分词结果中的暂停词
    if (stop_word_dict_.Find(word)) {
      continue;
    }
    ++word_cnt_map[word].content_cnt;

    // 更新当前词在正文中第一次出现的位置
    if (word_cnt_map[word].content_cnt == 1) {
      word_cnt_map[word].first_pos = token.beg();
    }
  }
  for (const auto& word_pair : word_cnt_map) {
    // 1. 拿着 doc_info 中的分词结果依次去 hash 表中查, 找到
    //    对应的倒排拉链(vector<Weight>)
    InvertedList& inverted_list
        = inverted_index_[word_pair.first];
    // 2. 根据当前 doc_info, 构造 Weight 对象.
    //    (需要计算权重和统计词出现的次数)
    
    // 构造Weight对象    
    Weight weight;
    weight.set_doc_id(doc_info.doc_id());
    weight.set_weight(CalcWeight(word_pair.second));
    weight.set_title_cnt(word_pair.second.title_cnt);
    weight.set_content_cnt(word_pair.second.content_cnt);
    weight.set_first_pos(word_pair.second.first_pos);
    // 3. 需要把 Weight 对象插入到刚刚获取到的倒排拉链中
    inverted_list.push_back(weight);
  }
  return;
}

int32_t Index::CalcWeight(const WordCnt& word_cnt) {
  // 简单权重计算公式
  return 10 * word_cnt.title_cnt + word_cnt.content_cnt;
}

void Index::SortInverted() {
  for (auto& invertd_pair : inverted_index_) {
     InvertedList& inverted_list = invertd_pair.second;
     std::sort(inverted_list.begin(),
         inverted_list.end(), CmpWeight);
  }
}

bool Index::CmpWeight(const Weight& w1, const Weight& w2) {
  // std::sort默认是按照小于比较(升序)
  // 为了完成降序排序
  return w1.weight() > w2.weight();
}

// 把内存中做好的索引结构写到磁盘上
bool Index::Save(const std::string& output_path) {
  // 1. 把内存中的结构构造成对应的 protobuf 结构
  std::string proto_data;
  CHECK(ConvertToProto(&proto_data));
  // 2. 基于 protobuf 结构进行序列化, 序列化的结果写到磁盘上
  CHECK(common::FileUtil::Write(output_path, proto_data));
  return true;
}

bool Index::ConvertToProto(std::string* proto_data) {
  doc_index_proto::Index index;
  // 1. 先将正排构造到 protobuf
  for (const auto& doc_info : forward_index_) {
    auto* proto_doc_info = index.add_forward_index();
    *proto_doc_info = doc_info;
  }
  // 2. 再将倒排构造到 protobuf
  for (const auto& inverted_pair : inverted_index_) {
    // inverted_pair.first 关键词
    // inverted_pair.second 倒排拉链(weight数组)
    auto* kwd_info = index.add_inverted_index();
    kwd_info->set_key(inverted_pair.first);
    for (const auto& weight : inverted_pair.second) {
      auto* proto_weight = kwd_info->add_weight();
      *proto_weight = weight;
    }
  }
  // 3. 使用 protobuf 对象进行序列化
  index.SerializeToString(proto_data);
  return true;
}

bool Index::Load(const std::string& index_path) {
  // 1. 读取文件内容, 基于 protobuf 进行反序列化
  std::string proto_data;
  CHECK(common::FileUtil::Read(index_path, &proto_data));
  // 2. 把 protobuf 结构转换回内存结构
  CHECK(ConvertFromProto(proto_data));
  return true;
}

bool Index::ConvertFromProto(const std::string& proto_data) {
  doc_index_proto::Index index;
  index.ParseFromString(proto_data);
  // 1. 把正排转换到内存中
  for (int i = 0; i < index.forward_index_size(); ++i) {
    const auto& doc_info = index.forward_index(i);
    forward_index_.push_back(doc_info);
  }
  // 2. 把倒排转换到内存中
  for (int i = 0; i < index.inverted_index_size(); ++i) {
    const auto& kwd_info = index.inverted_index(i);
    InvertedList& inverted_list
              = inverted_index_[kwd_info.key()];
    for (int j = 0; j < kwd_info.weight_size(); ++j) {
      const auto& weight = kwd_info.weight(j);
      inverted_list.push_back(weight);
    }
  }
  return true;
}

// 这个是用于辅助测试的接口. 
// 把内存中的索引结构遍历出来, 按照比较方便肉眼查看的格式打印到文件中
bool Index::Dump(const std::string& forward_dump_path, 
    const std::string& inverted_dump_path) {
  // 1. 处理正排
  std::ofstream forward_dump_file(forward_dump_path.c_str());
  CHECK(forward_dump_file.is_open());
  for (const auto& doc_info : forward_index_) {
    // Utf8DebugString protobuf 提供的接口, 构造出一个
    // 格式化的字符串方便肉眼观察
    forward_dump_file << doc_info.Utf8DebugString()
      << "\n-------------------\n";
  }
  forward_dump_file.close();
  // 2. 处理倒排
  std::ofstream inverted_dump_file(inverted_dump_path.c_str());
  CHECK(inverted_dump_file.is_open());
  for (const auto& inverted_pair : inverted_index_) {
    inverted_dump_file << inverted_pair.first << "\n";
    for (const auto& weight : inverted_pair.second) {
      inverted_dump_file << weight.Utf8DebugString();
    }
    inverted_dump_file << "\n------------------\n";
  }
  inverted_dump_file.close();
  return true;
}

const DocInfo* Index::GetDocInfo(uint64_t doc_id) const {
  // 非法ID
  if (doc_id >= forward_index_.size()) {
    return NULL;
  }
  return &forward_index_[doc_id];
}

const InvertedList* Index::GetInvertedList(
    const std::string& key) const {
  auto it = inverted_index_.find(key);
  if (it == inverted_index_.end()) {
    return NULL;
  }
  return &(it->second);
}

void Index::CutWordWithoutStopWord(const std::string& query,
    std::vector<std::string>* words) {
  // 先清空掉输出参数中原有的内容
  words->clear();
  // 调用 jieba 分词对应的函数来分词就行了
  std::vector<std::string> tmp;
  jieba_.CutForSearch(query, tmp);
  // 接下来需要把暂停词表中的词干掉
  // 遍历 tmp, 如果其中的分词结果不在暂停词表中, 
  // 才加入到最终结果
  for (std::string word : tmp) {
    // 由于搜索结果不区分大小写,先转为小写
    boost::to_lower(word);
    if (stop_word_dict_.Find(word)) {
      // 该分词结果是暂停词, 就不能放到最终结果中
      continue;
    }
    words->push_back(word);
  }
  return;
}
}  
