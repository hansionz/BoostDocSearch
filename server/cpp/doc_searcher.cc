#include "doc_searcher.h"
#include "../../common/util.hpp"

DEFINE_int32(desc_max_size, 160, "摘要的最大长度");

namespace doc_server {

bool DocSearcher::Search(const Request& req, Response* resp) {
  Context context;
  context.req = &req;
  context.resp = resp;
  // 1. 对查询词进行分词
  CutQuery(&context);
  // 2. 根据分词结果获取到倒排拉链
  Retrieve(&context);
  // 3. 根据触发的结果进行排序
  Rank(&context);
  // 4. 根据排序的结果, 包装响应
  PackageResponse(&context);
  // 5. 记录日志
  Log(&context);
  return true;
}

bool DocSearcher::CutQuery(Context* context) {
  // 基于 cppjieba 分词对查询词进行分词.
  // 分词结果, 就保存在 context->words
  // 当前的分词, 是需要隐蔽暂停词的.
  // "今天我吃饭了"
  // 暂停词表, 在索引中已经加载过一次了
  // 修改索引, 在索引中再新增一个 API 函数, 用来完成
  // 过滤暂停词的分词
  doc_index::Index* index = doc_index::Index::Instance();
  index->CutWordWithoutStopWord(context->req->query(),
      &context->words);
  return true;
}

bool DocSearcher::Retrieve(Context* context) {
  doc_index::Index* index = doc_index::Index::Instance();
  // 根据每个分词结果, 都去查倒排.
  // 把查到的倒排拉链, 统一放到 context->all_query_chain
  // 得到一个合并后的倒排拉链(包含了不同的关键词对应的拉链的集合)
  for (const auto& word : context->words) {
    const doc_index::InvertedList* inverted_list 
      = index->GetInvertedList(word);
    if (inverted_list == NULL) {
      // 当前关键词如果没在任何文档中出现过, 倒排拉链的结果就是 NULL.
      continue;
    }
    // 对当前获取到的倒排拉链进行合并操作
    for (const doc_index::Weight& weight : *(inverted_list)) {
      context->all_query_chain.push_back(weight);
    }
  }
  return true;
}

bool DocSearcher::Rank(Context* context) {
  // 直接按照 权重 来进行降序排序即可
  // all_query_chain 包含的是若干个关键词倒排拉链
  // 混合在一起的结果, 就需要进行重新的排序
  std::sort(context->all_query_chain.begin(),
      context->all_query_chain.end(),
      doc_index::Index::CmpWeight);
  return true;
}

// 小结, 此处通过 直接重新排序的方式 来实现触发和排序
// 固然可行, 但是还有更优化的做法.
// 前面已经是对每个倒排拉链排过序了, 现在是想得到一个仍然有序的大的序列
// 这里的问题场景就类似于把两个有序链表合并成一个有序链表只不过我们此处是把 N 个有序的 vector 合并成一个有序的 vector

bool DocSearcher::PackageResponse(Context* context) {
  // 构造出 Response 对象
  // Response 对象中最核心的是 Item 元素
  // 为了构造 Item 元素, 就需要查正排索引
  doc_index::Index* index = doc_index::Index::Instance();
  const Request* req = context->req;
  Response* resp = context->resp;
  resp->set_sid(req->sid());
  resp->set_timestamp(common::TimeUtil::TimeStamp());
  for (const auto& weight : context->all_query_chain) {
    // 根据 weight 中的 doc_id, 去查正排
    const doc_index::DocInfo* doc_info 
      = index->GetDocInfo(weight.doc_id());
    auto* item = resp->add_item();
    item->set_title(doc_info->title());
    // doc_info 中包含的是 标题 + 正文
    // item 中需要的是 标题 + 描述
    // 描述信息是正文的一段摘要. 这个摘要中需要包含
    // 查询词中的部分信息
    // 此处需要做的操作, 就是根据正文生成描述信息
    item->set_desc(GenDesc(doc_info->content(),
          weight.first_pos()));
    item->set_jump_url(doc_info->jump_url());
    item->set_show_url(doc_info->show_url());
  }
  return true;
}

// first_pos 中记录了当前关键词在文档正文中第一次出现的位置
// 根据这个位置, 把这个位置周边的一些内容提取出来, 作为描述
// 具体的提取策略如下:
// 示例: 乔布斯买了四斤香蕉, 乔布斯买了四斤苹果, 乔布斯又买了四斤鸭梨
// 对应的关键词是 "苹果", 获取到 first_pos
// 根据 first pos, 往前去查找, 查找 first_pos
// 所在的那句话的开始位置, 作为描述的开始位置.
// 从开始位置往后数160个字节(160是拍脑门出来的,
// 长度可以灵活调配), 作为结束位置
// 万一, 从描述开始位置计算, 剩余的正文内容不足 160,
// 直接就把剩下的所有信息作为描述 
// 如果从描述开始位置计算, 剩下的正文内容超过了 160, 
// 就只取 160 个字节, 并且把倒数3个字节设置为 '.'
std::string DocSearcher::GenDesc(const std::string& content,
    int first_pos) {
  int desc_beg = 0;
  // 根据 first_pos 去查找句子的开始位置
  // first_pos 表示的含义是当前词在正文中第一次出现的位置
  // 如果当前词只是在文档的标题中出现过而没在正文中出现过
  // 对于这种情况, first_pos 值是 -1
  if (first_pos != -1) {
    desc_beg = FindSentenceBeg(content, first_pos);
  }
  std::string desc;  // 保存描述结果
  if (desc_beg + FLAGS_desc_max_size
      >= (int64_t)content.size()) {
    // 说明剩下的内容不足以达到描述最大长度, 就把
    // 剩下的正文统统作为描述
    // 默认长度从开头取到末尾
    desc = content.substr(desc_beg);
  } else {
    // 剩下的内容超过了描述最大长度
    desc = content.substr(desc_beg, fLI::FLAGS_desc_max_size);
    // 需要把倒数三个字节设置为 .
    desc[desc.size() - 1] = '.';
    desc[desc.size() - 2] = '.';
    desc[desc.size() - 3] = '.';
  }
  // 接下来, 需要对描述中的特殊字符进行转换
  // 此处构造出的 desc 信息后续会作为 html 中的一部分
  // 就要求 desc 中不能包含 html 中的一些特殊字符
  ReplaceEscape(&desc);
  return desc;
}

int DocSearcher::FindSentenceBeg(const std::string& content,
    int first_pos) {
  // 从 first_pos 开始从后往前遍历, 找到第一个
  // 句子的分割符就可以了
  // , . ; ! ? 都是句子的分割符
  for (int cur = first_pos; cur >= 0; --cur) {
    if (content[cur] == ';'
        || content[cur] == ','
        || content[cur] == '.'
        || content[cur] == '!'
        || content[cur] == '?') {
      // cur 指向了上个句子的末尾, cur+1就是下个句子的开头
      return cur + 1;
    }
  }
  // 如果循环结束了还没找到句子的分割符, 取正文的开始位置(0)
  // 作为句子的起始
  return 0;
}

void DocSearcher::ReplaceEscape(std::string* desc) {
  // 需要替换的转义字符有以下几个:
  // " -> &quot;
  // & -> &amp;
  // < -> &lt;
  // > -> &gt;
  // 先替换 & , 避免后面把 &lt; 中的 & 再替换成 &amp;
  boost::algorithm::replace_all(*desc, "&", "&amp;");
  boost::algorithm::replace_all(*desc, "<", "&lt;");
  boost::algorithm::replace_all(*desc, ">", "&gt;");
  boost::algorithm::replace_all(*desc, "\"", "&quot;");
  return;
}

bool DocSearcher::Log(Context* context) {
  // 记录下一次请求过程中涉及到的请求详细信息和响应详细信息
  LOG(INFO) << "[Request]" << context->req->Utf8DebugString();
  LOG(INFO) << "[Response]" << context->resp->Utf8DebugString();
  return true;
}
}  // end doc_server
