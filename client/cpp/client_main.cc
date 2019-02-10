// 为了让搜索客户端能和 HTTP 服务器关联到一起, 就需要
// 让客户端能够支持 CGI 协议
#include <base/base.h>
#include <sofa/pbrpc/pbrpc.h>
#include "../../common/util.hpp"
#include "server.pb.h"
#include <iostream>

DEFINE_string(server_addr, "127.0.0.1:10000", "服务器端的地址");

namespace doc_client {

typedef doc_server_proto::Request Request;
typedef doc_server_proto::Response Response;

int GetQueryString(char output[]) {
  // 1. 先从环境变量中获取到方法
  char* method = getenv("METHOD");
  std::cerr << getenv("METHOD") << std::endl;;
  if (method == NULL) {
    fprintf(stderr, "METHOD failed\n");
    return -1;
  }
  // 2. 如果是 GET 方法, 就是直接从环境变量中
  //    获取到 QUERY_STRING
  if (strcasecmp(method, "GET") == 0) {
    char* query_string = getenv("QUERY_STRING");
    if (query_string == NULL) {
      fprintf(stderr, "QUERY_STRING failed\n");
      return -1;
    }
    strcpy(output, query_string);
  } else {
    // 3. 如果是 POST 方法, 先通过环境变量获取到 CONTENT_LENGTH
    //    再从标准输入中读取 body
    char* content_length_str = getenv("CONTENT_LENGTH");
    if (content_length_str == NULL) {
      fprintf(stderr, "CONTENT_LENGTH failed\n");
      return -1;
    }
    int content_length = atoi(content_length_str);
    int i = 0;  // 表示当前已经往  output 中写了多少个字符了
    for (; i < content_length; ++i) {
      read(0, &output[i], 1);
    }
    output[content_length] = '\0';
  }
  return 0;
}

void PackageRequest(Request* req) {
  // 此处先不考虑 sid 的生成方式
  req->set_sid(0);
  req->set_timestamp(common::TimeUtil::TimeStamp());
  char query_string[1024 * 4] = {0};
  GetQueryString(query_string);
  // 获取到的 query_string 的内容格式形如:
  // query=filesystem
  char query[1024 * 4] = {0};
  // 这是一个不太好的方案, 更严谨的方案是进行字符串切分
  sscanf(query_string, "query=%s", query);
  req->set_query(query);
  std::cerr << "query_string=" << query_string << std::endl;
  std::cerr << "query=" << query << std::endl;
}

void Search(const Request& req, Response* resp) {
  // 基于 RPC 方式调用服务器对应的 Search 函数
  using namespace sofa::pbrpc;
  // 1. PRC 概念1 RpcClient
  RpcClient client;
  // 2. PRC 概念2 RpcChannel, 描述一次连接
  // 相当于connect
  RpcChannel channel(&client, fLS::FLAGS_server_addr);
  // 3. RPC 概念3 DocServerAPI_Stub:
  //    描述你这次请求具体调用哪个RPC函数
  doc_server_proto::DocServerAPI_Stub stub(&channel);
  // 4. RPC 概念4 RpcController 网络通信细节的管理对象
  //    管理了网络通信中的五元组, 以及超时时间等概念
  RpcController ctrl;

  // 把第四个参数设为 NULL, 表示该请求按照同步的方式来进行请求
  stub.Search(&ctrl, &req, resp, NULL);
  if (ctrl.Failed()) {
    std::cerr << "RPC Add failed\n";
  } else {
    std::cerr << "RPC Add OK\n";
  }
  return;
}

// flask  template  {{占位}}
// ctemplate 
void ParseResponse(const Response& resp) {
  // std::cout << resp.Utf8DebugString();
  // 期望的输出结果是 HTML 格式的数据
  std::cout<< "<meta charset=\"UTF-8\">\n";
  std::cout<<"\n";  //http协议中的空行
  std::cout << "<html>";
  std::cout << "<body>";
  for (int i = 0; i < resp.item_size(); ++i) {
    const auto& item = resp.item(i);
    std::cout << "<div>";
    // 拼装标题和 jump_url 对应的 html
    std::cout << "<div><a href=\"" << item.jump_url()
              << "\">" << item.title() << "</a></div>";
    // 拼装描述
    std::cout << "<div>" << item.desc() << "</div>";
    // 拼装 show_url
    std::cout << "<div>" << item.show_url() << "</div>";
    std::cout << "</div>";
  }
  std::cout << "</body>";
  std::cout << "</html>";
}

// 此函数负责一次请求的完整过程
void CallServer(){
  // 1. 构造请求
  Request req;
  Response resp;
  PackageRequest(&req);
  // 2. 发送请求给服务器, 并获取到响应
  Search(req, &resp);
  // 3. 解析响应并打印结果
  ParseResponse(resp);
  return;
}
}  

int main(int argc, char* argv[]) {
  base::InitApp(argc, argv);
  doc_client::CallServer();
  return 0;
}
