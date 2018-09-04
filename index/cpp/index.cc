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
    : jieba_(fLS::FLAGS_dict_path,
             fLS::FLAGS_hmm_path,
             fLS::FLAGS_user_dict_path,
             fLS::FLAGS_idf_path,
             fLS::FLAGS_stop_word_path)
    {
        CHECK(stop_word_dict_.Load(fLS::FLAGS_stop_word_path));
    }

//从raw_input 文件当中读数据，在内存中构建索引结构
bool Index::Build(const std::string& input_path)
{
    LOG(INFO) << "Index Build";
    //1.按行读取文件内容，每一行都是一个文件
    std::ifstream file(input_path.c_str());
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

    //4. 处理完所有文档之后，针对所有的倒排拉链进行排序
    //   key-value中的value进行排序，按照权重降序排序
    SortInverted();
    file.close();
    LOG(INFO) << "Index Build Done!!!";
    return true;
}

const DocInfo* Index::BuildForward(const std::string& line)
{
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
    doc_info.set_jump_url(tokens[0]);
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
    return &forward_index_.back();

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

    for(size_t i = 0; i < words.size(); ++i)
    {
        //先创建一个title_token的pair的空间，然后再给这个空间赋值
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

void Index::SplitContent(const std::string& content, DocInfo* doc_info)
{
    std::vector<cppjieba::Word> words;
    //要调用cppjieba进行分词，需要创建一个jieba对象
    jieba_.CutForSearch(content, words);
    //words里面包含包含的分词结果，每个结果包含一个 offset
    //offset表示的是当前词在文档中的起始位置的下标
    //而世界上这里我们需要的是前闭后开区间
    if(words.size() <= 1)
    {
        //错误处理
        LOG(FATAL) << "SplitContent failed!";
        return;
    }

    for(size_t i = 0; i < words.size(); ++i)
    {
        //先在doc_info里面的content_token数组中创建一个pair空间
        auto* token = doc_info->add_content_token();
        token->set_beg(words[i].offset);
        if(i + 1 < words.size())
        {
            //i 不是最后一个元素
            token->set_end(words[i+1].offset);
        }
        else
        {
            //i 是最后一个元素
            token->set_end(content.size());
        }
    }

    return;
}


void Index::BuildInverted(const DocInfo& doc_info)
{
    WordCntMap word_cnt_map; //key为关键词，value为结构体，结构体的内容为，词在正文，标题的出现次数
    //1. 统计 title 中每个词出现的次数
    for(int i = 0; i < doc_info.title_token_size(); ++i)
    {
        const auto& token = doc_info.title_token(i);
        //因为这里的token数组里面存的区间，真正的内容在title中
        std::string word = doc_info.title().substr(token.beg(), token.end() - token.beg());

        //HELLO hello在这里算一个词，大小写不敏感
        boost::to_lower(word);

        //去掉暂停词
        if(stop_word_dict_.Find(word))
        {
            continue;
        }

        //存在，值就++；否则，插入
        ++word_cnt_map[word].title_cnt;
        
    }

    //2. 统计content中每个词出现的个数
    //   此时得到一个hash表，哈市表中的key就是关键词，
    //   hash表中的value就是一个结构体，结构体中包含
    //   该词在标题和正文中出现的次数
    for(int i = 0; i < doc_info.content_token_size(); ++i)
    {
        const auto& token = doc_info.content_token(i);
        std::string word = doc_info.content().substr(token.beg(), token.end() - token.end());
        boost::to_lower(word);
        if(stop_word_dict_.Find(word))
        {
            continue;
        }
        ++word_cnt_map[word].content_cnt;
        //记录词在正文中第一次出现的位置--方便以后返回响应的时候构建描述信息
        if(word_cnt_map[word].content_cnt == 1)
        {
            word_cnt_map[word].first_pos = token.beg();
        }
    }
    //3. 根据统计结果，更新到倒排索引InvertedIndex中
    //   遍历刚才的hash表，拿着key去倒排索引中查找
    //   如果倒排索引中不存在这个词，就新增一项
    //   否则，就构造一个Weight结构，将其添加到
    //   对应的倒排拉链InvertedList当中
    for(const auto& word_pair : word_cnt_map)
    {
        Weight weight;
        weight.set_doc_id(doc_info.id());
        //这里构造Weight结构，得先计算权重，second为value，first为key
        weight.set_weight(CalcWeight(word_pair.second.title_cnt, word_pair.second.content_cnt));
        weight.set_first_pos(word_pair.second.first_pos);

        //先获取到当前词对应的倒排拉链
        InvertedList& inverted_list = inverted_index_[word_pair.first];
        inverted_list.push_back(weight);
    }

    return;
}

int Index::CalcWeight(int title_cnt, int content_cnt)
{
    //这里权重直接简单的进行计算
    return 10 * title_cnt + content_cnt;
}

void Index::SortInverted()
{
    //把所有的倒排拉链都按照weight降序排序
    for(auto& inverted_pair : inverted_index_)
    {
        //每个inverted_pair时一个键值对
        //key为关键词，value为weight的数组
        //每个weight里面为文档id和权重
        InvertedList& inverted_list = inverted_pair.second;
        std::sort(inverted_list.begin(), inverted_list.end(), CmpWeight);
    }
    return;
}

bool Index::CmpWeight(const Weight& w1, const Weight& w2)
{
    return w1.weight() > w2.weight();
}

//把内存中的索引数据保存到磁盘中
bool Index::Save(const std::string& ouput_path)
{
    LOG(INFO) << "Index Save";
    //1. 把内存中的索引结构序列化成字符串
    std::string proto_data;
    CHECK(ConvertToProto(&proto_data));
    //2. 把序列化得到的字符串写到文件中
    CHECK(common::FileUtil::Write(ouput_path, proto_data));
    LOG(INFO) << "Index Save Done";

    return true;
}

bool Index::ConvertToProto(std::string* proto_data)
{
    //index.proto中的命名空间
    doc_index_proto::Index index; //index中包含正排索引和倒排索引(倒排索引也是数组，由于protobuf中没有hash的类型)
    //需要把内存中的数据设置到 index 中
    //1. 设置正排
    for(const auto& doc_info : forward_index_)
    {
        //创建一个空间
        auto* proto_doc_info = index.add_forward_index();
        //给空间赋值
        *proto_doc_info = doc_info;
    }

    //2. 设置倒排
    for(const auto& inverted_pair : inverted_index_)
    {
        //创建空间，每一个空间为键值对结构
        auto* kwd_info = index.add_inverted_index();
        kwd_info->set_key(inverted_pair.first);
        //value为Weight结构的数组，里面包含文档id和权重
        for(const auto& weight : inverted_pair.second)
        {
            //创建倒排拉链的空间
            auto* proto_weight = kwd_info->add_doc_list();
            *proto_weight = weight;
        }
    }

    //序列化，将序列化好的字符串保存在proto_data中
    index.SerializeToString(proto_data);
    return true;
}

//把磁盘上的文件加载到内存的索引结构中
bool Index::Load(const std::string& index_path)
{
    LOG(INFO) << "Index Load";
    std::cout << "Into Load\n";
    //1. 从磁盘上把索引文件读到内存中
    std::string proto_data;
    CHECK(common::FileUtil::Read(index_path, &proto_data));
    std::cout << "read over\n";
    //2. 进行反序列化，转成内存中的索引结构
    CHECK(ConvertFromProto(proto_data));
    LOG(INFO) << "Index Load Done";
    return true;
}

bool Index::ConvertFromProto(const std::string& proto_data)
{
    //1. 对索引文件内容进行反序列化
    doc_index_proto::Index index;
    index.ParseFromString(proto_data);
    //2. 已经反序列化好的数据在index中，
    //   将index中的内容构建成内存中的索引结构
    //   构建正排索引forward_index_
    for(int i = 0; i < index.forward_index_size(); ++i)
    {
        const auto& doc_info = index.forward_index(i);
        forward_index_.push_back(doc_info);
    }

    //3. 构建倒排索引inverted_index_
    for(int i = 0; i < index.inverted_index_size(); ++i)
    {
        //kwd_info为结构体，包含关键词key和weight数组(倒排拉链)
        const auto& kwd_info = index.inverted_index(i);
        //找到key在inverted_index_(内存中的倒排索引)
        //对应的倒排拉链,然后再把对应的weight插入进去
        InvertedList& inverted_list = inverted_index_[kwd_info.key()];
        for(int j = 0; j < kwd_info.doc_list_size(); ++j)
        {
            const auto& weight = kwd_info.doc_list(j);
            inverted_list.push_back(weight);
        }
    }
    return true;
}

//调试用的接口，把内存中的索引结构按照一定的格式打印到文件中
bool Index::Dump(const std::string& forward_dump_path, const std::string& inverted_dump_path)
{
    LOG(INFO) << "Index dump";
    //1. 处理正排
    std::ofstream forward_dump_file(forward_dump_path.c_str());
    CHECK(forward_dump_file.is_open());
    for(size_t i = 0; i < forward_index_.size(); ++i)
    {
        const DocInfo& doc_info = forward_index_[i];
        forward_dump_file << doc_info.Utf8DebugString() << "====================";
    }

    forward_dump_file.close();

    //2. 处理倒排
    std::ofstream inverted_dump_file(inverted_dump_path.c_str());
    CHECK(inverted_dump_file.is_open());
    for(const auto& inverted_pair : inverted_index_)
    {
        inverted_dump_file << inverted_pair.first << "\n";
        for(const auto& weight : inverted_pair.second)
        {
            inverted_dump_file << weight.Utf8DebugString();
        }
        inverted_dump_file << "===================";
    }
    inverted_dump_file.close();
    LOG(INFO) << "Index Dump Done";
    return true;
}

//根据 doc_id 获取到文档详细信息
const DocInfo* Index::GetDocInfo(uint64_t doc_id) const
{
    if(doc_id >= forward_index_.size())
    {
        return NULL;
    }

    return &forward_index_[doc_id];
}

const InvertedList* Index::GetInvertedList(const std::string& key) const
{
    //这里的find找到后会返回对应的迭代器
    //但是[]返回的是valuei,并且find找不到
    //的话会返回end可以用来判断是否存在
    //但是[]不存在的话就直接插入了
    auto it = inverted_index_.find(key);
    if(it == inverted_index_.end())
    {
        return NULL;
    }

    return &(it->second);
}

//此处为了方便服务器进行分词，再提供一个函数
//需要把所有的暂停词从分词结果中过滤掉
void Index::CutWordWithoutStopWord(const std::string& query, std::vector<std::string>* words)
{
    //将最后的分词结果保存再word中
    words->clear();
    std::vector<std::string> tmp;
    //由于分完词之后，暂停词还在
    //这里我们需要将暂停词去掉放到word中
    jieba_.CutForSearch(query, tmp);
    for(std::string& token : tmp)
    {
        //判定是否为暂停词对大小写不敏感
        boost::to_lower(token);
        if(stop_word_dict_.Find(token))
        {
            continue;
        }
        words->push_back(token);
    }
}


} //end doc_index



