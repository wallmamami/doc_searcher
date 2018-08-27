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
};
} //end common


