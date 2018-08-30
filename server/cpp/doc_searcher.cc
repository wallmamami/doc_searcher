#include "doc_searcher.h"
#include <base/base.h>

DEFINE_int32(desc_max_size, 160, "描述的最大长度");

namespace doc_server
{
    
//搜索流程的入口函数
//req中主要包含请求字符串query
//resp中主要半酣item数组
//item中为标题，正文描述，跳转、展示url
bool DocSearcher::Search(const Request& req, Response* resp)
{
    Context context(&req, resp);
    //1. 对查询词进行分词
    CutQuery(&context);
}

//对查询词进行分词
bool CutQuery(Context* context)
{
    // 调用当时索引结构栈总提供的接口
    // 因为直接调用只是分词，并不会
    // 去掉暂停词
    Index* index = Index::Indstance();
    index->CutWordWithoutStopWord(context->req->query(), &context->words);
    LOG(INFO) << "CutQuery Done! sid=" << context->req->sid();
    return true;
}

//根据查询词结果进行触发
bool Retrieve(Context* context)
{
    //根据分词结果，到索引中找到所有的倒排拉链
    //然后将倒排拉链插入到context中的
    //all_query_chain(用来保存所有weight的数组)
    
}

    //根据触发结果进行排序
    bool Rank(Context* context);
    //根据排序的结果拼装成响应
    bool PackageResponse(Context* context);
    //打印请求日志
    bool Log(Context* context);
    //排序需要的比较函数
    static bool CmpWeightPtr(const Weight* w1, const Weight* w2);
    //生成描述信息
    std::string GenDesc(int first_pos, const std::string& content);
    //替换html中的转义字符
    void ReplaceEscape(std::string* desc);
} // end doc_server
