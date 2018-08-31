#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <boost/algorithm/string.hpp>
#include <sys/time.h>


namespace common
{

class StringUtil
{
public:
    //关闭压缩
    //输入字符串 aaa
    //对于这个字符串，如果关闭压缩，切分结果返回三个部分 aaa NULL NULL
    //如果打开压缩，切分结果为两个部分，把两个相邻的合并为1个 aaa NULL
    static void Split(const std::string& input, std::vector<std::string>* output, const std::string& split_char)
    {
        boost::split(*output, input, boost::is_any_of(split_char), boost::token_compress_off);
    }

    static int32_t FindSentenceBeg(const std::string& content, int32_t first_pos)
    {
        //从first_pos位置超前找，不能用.作为句子
        //的结束标志，因为可能存在以下情况
        //s.begin()，但是如果是.后面还跟着空格就表示
        //句子结束了
        for(int32_t i = first_pos; i >= 0 ; --i)
        {
            if(content[i] == ';' || content[i] == ',' ||
               content[i] == '?' || content[i] == '!' ||
               (content[i] == '.' && content[i+1] == ' '))
            {
                return i + 1;
            }
        }
        //如果没有找到结束标志，表示这是句子第一句话
        return 0;
    }
};

class DicUtil
{
public:
    //从cppjieba暂停词文件中加载暂停词的内容
    //一行为一个暂停词，这里将一个暂停词作为set的一个元素
    bool Load(const std::string& path)
    {
        std::ifstream file(path.c_str());
        if(!file.is_open())
        {
            //这里打印日志不适用LOG(FATAL)，主要是因为这个类
            //可能会在很多不同场景下使用到，所以错误级别交给调用者来决定
            return false;
        }

        std::string line;
        while(std::getline(file, line))
        {
            set_.insert(line);
        }

        return true;
    }

    bool Find(const std::string& key)const
    {
        return set_.find(key) != set_.end();
    }

private:
    std::unordered_set<std::string> set_;
};

class FileUtil
{
public:
    //把 content 中的所有内容都写到文件当中
    static bool Write(const std::string& output_path, const std::string& content)
    {
        std::ofstream file(output_path.c_str());
        if(!file.is_open())
        {
            return false;
        }

        file.write(content.data(), content.size());
        file.close();
        return true;
    }

    //把文件的所有内容都读到content中
    static bool Read(const std::string& input_path, std::string* content)
    {
        std::ifstream file(input_path.c_str());
        if(!file.is_open())
        {
            return false;
        }

        //需要先获取到文件的长度
        //把文件光标放到文件末尾
        file.seekg(0, file.end);
        //获取到文件光标的位置
        int len = file.tellg();
        file.seekg(0, file.beg);
        content->resize(len);
        file.read(const_cast<char*>(content->data()), len);
        file.close();
        return true;
    }
};

class TimeUtil
{
public:
    //获取到秒级时间戳
    static int64_t TimeStamp()
    {
        struct timeval tv;
        ::gettimeofday(&tv, NULL);
        return tv.tv_sec;
    }

    //获取到毫秒时间戳
    static int64_t TimeStampMS()
    {
        struct timeval tv;
        ::gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }

    //微秒时间戳
    static int64_t TimeStampUS()
    {
        struct timeval tv;
        ::gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
    }
};

} //end common


