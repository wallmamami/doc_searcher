#include <base/base.h>
#include <sofa/pbrpc/pbrpc.h>
#include <ctemplate/template.h>
#include "../../common/util.hpp"
#include "server.pb.h"



DEFINE_string(server_addr, "127.0.0.1:10000","请求的搜索服务器的地址");
DEFINE_string(template_path,"../template/search_page.html","模板文件的路径");

namespace doc_client
{

typedef doc_server_proto::Request Request;
typedef doc_server_proto::Response Response;


int GetQueryString(char output[]) 
{
    //1. 先从环境变量中获取到方法
    char* method = getenv("METHOD");
    if (method == NULL) 
    {
        fprintf(stderr, "REQUEST_METHOD failed\n");
        return -1;
    }
    // 2. 如果是 GET 方法, 就是直接从环境变量中
    //    获取到 QUERY_STRING
    if (strcasecmp(method, "GET") == 0) 
    {
        char* query_string = getenv("QUERY_STRING");
        if (query_string == NULL) 
        {
            fprintf(stderr, "QUERY_STRING failed\n");
            return -1;
        }
        strcpy(output, query_string);
    } 
    else 
    {
        // 3. 如果是 POST 方法, 先通过环境变量获取到 CONTENT_LENGTH
        //    再从标准输入中读取 body
        char* content_length_str = getenv("CONTENT_LENGTH");
        if (content_length_str == NULL) 
        {
            fprintf(stderr, "CONTENT_LENGTH failed\n");
            return -1;
        }
        int content_length = atoi(content_length_str);
        int i = 0;  // 表示当前已经往  output 中写了多少个字符了
        for (; i < content_length; ++i) 
        {
            read(0, &output[i], 1);
        }
        output[content_length] = '\0';
    }
    return 0;
}

void PackageRequest(Request* req)
{
    // 这里sid的生成先不考虑
    req->set_sid(0);
    req->set_timestamp(common::TimeUtil::TimeStamp());

    //这里的查询词要从环境变量中获取
    char buf[1024] = {0};
    GetQueryString(buf);
    //现在buf中就有query=hah字符串了
    char query[1024] = {0};
    sscanf(buf, "query=%s", query);
    req->set_query(query);
}

void Search(const Request& req, Response* resp)
{
    //这里的函数实际上调用的是RPC
    //框架中的服务器端的同名函数
    using namespace sofa::pbrpc;
    //调用的过程
    //1. 先定义一个RPC client对象
    RpcClient client;
    //2. 再定义一个RpcChannel对象，描述了一个连接
    RpcChannel channel(&client, fLs::FLAGS_server_addr);
    //3. 再定义一个DocServerAPI_Stub，用来表示
    //   调用服务器的哪一个函数
    doc_server_proto::DocServerAPI_Stub stub(&channel);
    //4. 再定义一个ctrl对象，用来网络控制的对象
    RpcController ctrl;
    ctrl.SetTimeout(3000);//3000毫秒
    //5. 远程调用服务器端的Search函数。这里在客户端
    //   本地调用就相当于调用到远端服务器的函数了
    stub.Search(&ctrl, &req, resp, NULL);
    //6. 判断是否调用成功
    if(ctrl.Failed())
    {
        std::cerr << "RPC Search failed" << std::endl;

    }
    else
    {
        std::cerr << "RPC Search OK" << std::endl;
    }
}

void ParseResponse(resp)
{
    // 返回的响应的结果是HTML
    // 此处使用ctemplate完成页面构造
    // 目的是为了HTML所描述的界面和cpp的逻辑拆分开
    ctemplate::TemplateDictionary dict("SearchPage");
    for(int i = 0; i < resp.item_size(); ++i)
    {
        ctemplate::TemplateDictionary* table_dict = dict.AddSectionDictionary("item");
        table_dict->SetValue("title", resp.item(i).title());
        table_dict->SetValue("desc", resp.item(i).desc());
        table_dict->SetValue("jump_url", resp.item(i).jump_url());
        table_dict->SetValue("show_url", resp.item(i).show_url());
    }

    // 把模板文件加载起来
    ctemplate::Template* tpl = ctemplate::Template::GetTemplate(fLS::FLAGS_template_path, ctemplate::DO_NOT_STRIP);
    // 对模板进行替换
    std::string html;
    tpl->Expand(&html, &dict);
    std::cout << html;
    return;
}

//此函数位客户端请求服务器的入口函数
void CallServer()
{
    //1. 构建请求并发送给服务器
    Request req;
    Response resp;
    PackageRequest(&req);
    //2. 调用本地Search函数，本地远程调用
    //   服务器端的，获取到响应并解析响应
    Search(req, &resp);

    //3. 解析响应并输出结果，由于这个客户端
    //   就是cgi程序，这里输出就相当于经过http
    //   服务器返回给浏览器
    ParseResponse(resp);
    return;
}


}//end doc_client


int main(int argc, char* argv[])
{
    base::InitApp(argc, argv);
    doc_client::CallServer();
    return 0;
}
