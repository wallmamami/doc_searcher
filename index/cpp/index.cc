#include <fstream>
#include <base/base.h>
#include "index.h"

// jieba 依赖的字典路径
DEFINE_string(dict_path, "../../third_part/data/jieba_dict/jieba.dict.utf8", "字典路径");
DEFINE_string(hmm_path, "../../third_part/data/jieba_dict/hmm_model.utf8", "hmm 字典路径");
DEFINE_string(user_dict_path, "../../third_part/data/jieba_dict/user.dict.utf8", "用户自定制词典路径");
DEFINE_string(idf_path, "../../third_part/data/jieba_dict/idf.utf8", "idf 字典路径");
DEFINE_string(stop_word_path, "../../third_part/data/jieba_dict/stop_words.utf8", "暂停词字典路径");

namespace doc_index
{
    
Index* Index::inst_ = NULL;

Index::Index()
{}

//从raw_input 文件当中读数据，在内存中构建索引结构
bool Index::Build(const std::string& input_path)
{
    LOG(INFO) << "Index Build";
    //1.按行读取文件内容，每一行都是一个文件
    std::ifstream file(input_path.c_st());
    CHECK(file.is_open()) << "input_path:" << input_path;
    std::string line;
    while(std::getline(file, line))
    {
        //2.把一行数据（代表一个正文）制作成一个DocInfo(正排索引数组的元素类型)
        //  此处获取到的doc_info 是为了接下来制作倒排方便
        const DocInfo* doc_info = BuildForward(line);
        //如果构建失败，立刻终止进程
        CHECK(doc_info != NULL);
        //3. 更新倒排信息
        //   此函数的输出结果，直接放到Index::inverted_index_中
        BuildInverted(*doc_info);
    }
}

const DocInfo* Index::BuildForward(const std::string& line)
{
    //1. 先对字符串进行切分(自己定义的\3)
    std::vector<std::string> tokens;
    //当前Split 不会破坏原字符串
    common::StringUtil::Split(line, &tokens, "\3");
    if(tokens.size() != 3)
    {
        LOG(FATAL) << "line split not 3 tokens! tokens.size() = " << tokens.size();
        return NULL;
    }

    //切分成功后的tokens，保存了id,title,content等信息(每个元素为一个信息)，id为正排数组的size
    //2. 构造一个DocInfo结构，把切分的结果赋值到DocInfo
    //   除了分词结果之外都能直接进行赋值
    DocInfo doc_info;
    doc_info.set_id(forward_index_.size());
    doc_info.set_title(tokens[1]);
    doc_info.set_content(tokens[2]);
    doc_info.set_jump_url(tokens[3]);
    //这里为了方便，将show_url与jump_url设置为一样
    //实际上show_url只包含jump_url的域名
    doc_info.set_show_url(doc_info.jump_url());
    
    //3. 这里为了方便倒排，将标题和正文的分词结果保存在doc_info中的
    //   title_token与content_token中(为左闭右开的区间)
    //   doc_info是输出型参数，用指针的方式传入
    SplitTitle(tokens[1], &doc_info);
    SplitContent(tokens[2], &doc_info);

    //4. 将这个DocInfo 插入到正排索引中
    forward_index_.push_back(doc_info);
    return &forward_index_back();

}

void Index::SplitTitle(const std::string& title, DocInfo* doc_info)
{
    std::vector<cppjieba::Word> words;
    //要调用 cppjieba 进行分词，需要先创建一个jieba对象
    jieba_.CutForSearch(title, words);
    //words里面包含的分词结果，每一个结果包含一个offset。
    //offset表示的是当前词在文档中的其实位置的下标，但是这里
    //我们需要的是前闭后开区间
    if(words.size() < 1)
    {
        //错误处理
        LOG(FATAL) << "SplitTitle failed! title = " << title;
        return;
    }

    for(size_t i = 0; i < word.size(); ++i)
    {
        auto* token = doc_info->add_title_token();
        token->set_beg(words[i].offset);
        if(i + 1 < words.size())
        {
            //i 不是最后一个元素
            token->set_end(words[i+1].offset);
        }
        else
        {
            //i 是最后一个元素
            token->set_end(title.size());
        }
    }

    return;
}

} //end doc_index



