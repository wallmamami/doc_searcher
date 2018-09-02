#include <base/base.h>
#include <sofa/pbrpc/pbrpc.h>
#include "../../common/util.hpp"
#include "server.pb.h"
#include "doc_searcher.h"


DEFINE_string(port, "10000", "服务器端口号");
DEFINE_string(index_path, "../index/index_file","索引文件的路径");

namespace doc_server 
{

typedef doc_server_proto::Request Request;
typedef doc_server_proto::Response Response;

class DocServerAPIImpl : public doc_server_proto::DocServerAPI 
{
public:
        //客户端在本地调用的函数，实际上调用的是服务器本地
        //的函数，此函数是真正在服务器端完成计算的函数
        void Search(::google::protobuf::RpcController* controller, const Request* req, Response* resp,::google::protobuf::Closure* done) 
        {
            (void) controller;

            resp->set_sid(req->sid());
            resp->set_timestamp(common::TimeUtil::TimeStamp());
            // resp->set_err_code(0);

            // 具体如何完成更详细的搜索计算, 一会再说
            DocSearcher searcher;
            searcher.Search(*req, resp);

            // 这行代码表示服务器对这次请求的计算就完成了.
            // 由于 RPC 框架一般都是在服务器端异步完成计算,
            // 所以就需要由被调用的函数来通知调用者我计算完了.
            done->Run();
        }
};

} //end doc_server


int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
  	fLS::FLAGS_log_dir = "../log/"; 
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    using namespace sofa::pbrpc;

    //1. 加载初始化索引模块(在Index模块中已经生成索引结构并保存在文件当中)
    doc_index::Index* index = doc_index::Index::Instance();
    CHECK(index->Load(fLS::FLAGS_index_path));
    LOG(INFO) << "Index Load Done !";
    //1. 定义一个 RpcServerOptions 对象
    //   这个对象描述了RPC服务器一些相关选项
    //   主要是为了定义线程池中线程的个数
    RpcServerOptions option;
    option.work_thread_num = 4;
    //2. 定义一个RpcServer对象(和ip端口号关联起来)
    RpcServer server(option);
    CHECK(server.Start("0.0.0.0:" + fLS::FLAGS_port));
    //3. 定义一个 DocServerAPIImol，并注册到RpcServer对象中
    doc_server::DocServerAPIImpl* service_impl = new doc_server::DocServerAPIImpl();
    server.RegisterService(service_impl);
    //4. 让 RpcServer 对象开始执行
    server.Run();
    return 0;
}
