# 基于boost文档的小型搜索引擎

**** 

### 模块划分

* HTTP服务器模块(使用CGI程序进行程序替换)
* 索引模块(包括正排索引和倒排索引)
* CGI客户端
* 搜索服务器

### 项目核心流程

* 用户在浏览器输入关键词,然后浏览器向HTTP服务器通过GET或POST方法发送请求
* HTTP服务器通过替换CGI程序将请求封装成一个TCP报文发送给搜索服务器
* 搜索服务器将索引模块已经制作好的索引文件加载到内存中
* 搜索服务器先将关键词进行分词操作，然后通过封装的静态库(包含操作索引文件的API接口)来实现查询
* 然后搜索服务器将查询结果封装成TCP响应返回给CGI客户端
* CGI程序将返回结果封装成HTML页面通过管道发送给HTTP服务器中的父进程
* 最后HTTP服务器将响应结果发给浏览器，浏览器就可以获得展现查询结果

**注:CGI程序和搜索服务器的通信借助于baidu-sofa实现，隐藏网络通信的细节.**

### 项目使用的第三方库

* protobuf使用说明:
  https://www.cnblogs.com/yinheyi/p/6080244.html
	https://developers.google.com/protocol-buffers

* gflags使用说明:
  https://www.cnblogs.com/SarahLiu/p/7121385.html

* glog使用说明:
  https://www.cnblogs.com/qiumingcheng/p/8182771.html

* cppjieba分词使用说明:(官方文档)
  https://github.com/yanyiwu/cppjieba/blob/master/test/demo.cpp