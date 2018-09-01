#include "doc_searcher.h"
#include <base/base.h>

DEFINE_int32(desc_max_size, 160, "描述的最大长度");

namespace doc_server
{

//搜索流程的入口函数
//req中主要包含请求字符串query
//resp中主要包含item数组
//item中为标题，正文描述，跳转、展示url
bool DocSearcher::Search(const Request& req, Response* resp)
{
    //context里面包含请求和响应，以及
    //请求的分词结果(用vector保存)和分词
    //结果对应的所有倒排拉链(Weight*数组)
    Context context(&req, resp);
    //1. 对查询词进行分词
    CutQuery(&context);
    //2. 根据分词结果进行触发
    Retrieve(&context);
    // 3. 根据触发结果进行排序
    Rank(&context);
    // 4. 根据排序结果进行包装响应
    PackageResponse(&context);
    // 5. 记录处理日志
    Log(&context);
    return true;
}

//对查询词进行分词
bool DocSearcher::CutQuery(Context* context)
{
    // 调用当时索引结构栈总提供的接口
    // 因为直接调用只是分词，并不会
    // 去掉暂停词
    Index* index = Index::Instance();
    index->CutWordWithoutStopWord(context->req->query(), &context->words);
    LOG(INFO) << "CutQuery Done! sid=" << context->req->sid();
    return true;
}

//根据查询词结果进行触发
bool DocSearcher::Retrieve(Context* context)
{
    Index* index = Index::Instance();
    //根据分词结果，到索引中找到所有的倒排拉链
    //然后将倒排拉链插入到context中的
    //all_query_chain(用来保存所有weight的数组)
    for(const auto& word : context->words)
    {
        const doc_index::InvertedList* inverted_list = index->GetInvertedList(word);
        if(inverted_list == NULL)
        {
            //该分词结果如果没有对应的倒排拉链，继续
            //不影响其他的
            continue;
        }
        for(size_t i = 0; i < inverted_list->size(); ++i)
        {
            const auto& weight = (*inverted_list)[i];
            context->all_query_chain.push_back(&weight);
        }
    }

    return true;
}

//根据触发结果进行排序
bool DocSearcher::Rank(Context* context)
{
    //虽然之前再索引结构中已经对每个倒排拉链排过序了
    //但是all_query_chain保存了多个倒排拉链，所以还要
    //对其进行排序，规则也是按照权重降序排序
    std::sort(context->all_query_chain.begin(), context->all_query_chain.end(), CmpWeightPtr);

    return true;
}

//排序需要的比较函数
bool DocSearcher::CmpWeightPtr(const Weight* w1, const Weight* w2)
{
    return w1->weight() > w2->weight();
}

//根据排序的结果拼装成响应
bool DocSearcher::PackageResponse(Context* context)
{
    //构造出最终的Response结构
    //resp中主要包含item数组
    //item中为标题，正文描述，跳转、展示url
    Index* index = Index::Instance();
    const Request* req = context->req;
    Response* resp = context->resp;
    resp->set_sid(req->sid());
    resp->set_timestamp(common::TimeUtil::TimeStamp());
    resp->set_err_code(0);

    //根据context中的all_query_chain中的weight结构，
    //拿到doc_id,再到正排索引中查找到文档的详细信息
    //doc_info(标题，正文，show_url，jump_url)
    for(const auto* weight : context->all_query_chain) 
    {
        const auto* doc_info = index->GetDocInfo(weight->doc_id());
        //使用doc_info构建响应中的item(doc_info与item一一对应)
        auto* item = resp->add_item();
        item->set_title(doc_info->title());
        //item中的描述是根据doc_info中的正文生成的
        //而不是直接将正文设置
        //这里在生成描述的时候，由于不知道是词与正文的对应
        //关系，所以就用到了weight中词在正文第一次出现的位置
        //来构建
        item->set_desc(GenDesc(weight->first_pos(), doc_info->content()));
        item->set_jump_url(doc_info->jump_url());
        item->set_show_url(doc_info->show_url());
    }
    return true;
}

//这里描述的构建比较灵活，只要用户体验好，都行，
//根据first_pos找到句子开始位置，然后设置描述最大
//长度，描述就构建成功了
std::string DocSearcher::GenDesc(int first_pos, const std::string& content)
{
    //1. 根据first_pos找到这句话的开始位置
    //  通过标点符号来确定。
    int64_t desc_beg = 0;
    //这里first_pos可能不存在，表示这个词没在正文中出现过
    //如果是这只情况，直接从正文开始位置构建描述
    if(first_pos != -1)//表示存在
    {
        desc_beg = common::StringUtil::FindSentenceBeg(content, first_pos);
    }
    //2. 从句子开始的位置，往后去找若干字节(可以自定义)
    std::string desc;
    //3. 如果句子开始到正文结束，长度毒狗
    //   自定义的描述长度，就将将整体作为描述
    if(desc_beg + fLI::FLAGS_desc_max_size >= (int32_t)content.size())
    {
        //没有结尾表示到string 的结尾
        desc = content.substr(desc_beg);
    }
    else
    {
        //4. 如果句子开始到正文结尾超过自定义长度
        //   则将倒数三个字节改成...，类似于省略号
        desc = content.substr(desc_beg, fLI::FLAGS_desc_max_size);
        desc[desc.size() - 1] = '.';
        desc[desc.size() - 2] = '.';
        desc[desc.size() - 3] = '.';
    }

    //由于返回的是html文件，所以需要将特殊字符
    //替换成转义字符
    ReplaceEscape(&desc);
    return desc;
}

//替换html中的转义字符
void DocSearcher::ReplaceEscape(std::string* desc)
{
    boost::algorithm::replace_all(*desc, "&", "&amp;");
    boost::algorithm::replace_all(*desc, "\"", "&quot;");
    boost::algorithm::replace_all(*desc, "<", "&lt;");
    boost::algorithm::replace_all(*desc, ">", "&gt;");
}
//打印请求日志
//生成描述信息
bool DocSearcher::Log(Context* context)
{
    LOG(INFO) << "[Request]" << context->req->Utf8DebugString();
    LOG(INFO) << "[Response]" << context->resp->Utf8DebugString();
    return true;
}

} // end doc_server
