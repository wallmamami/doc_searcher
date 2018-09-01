#include <base/base.h>
#include "index.h"


DEFINE_string(input_path, "../data/tmp/raw_input", "raw_input 文件路径");
DEFINE_string(output_path, "../data/output/index_file", "索引文件输出路径");

int main(int argc, char* argv[]) 
{
    google::InitGoogleLogging(argv[0]);
  	fLS::FLAGS_log_dir = "../log/"; 
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    doc_index::Index* index = doc_index::Index::Instance();
    CHECK(index->Build(fLS::FLAGS_input_path));
    CHECK(index->Save(fLS::FLAGS_output_path));
    return 0;

}
