#include "edgequant/args_parser.h"
#include <iostream>
#include <CLI/CLI.hpp> 

namespace edgequant {

Args parse_arguments(int argc, char* argv[]) {
    Args args;

    CLI::App app{"EdgeQuant Toolkit"};
    app.set_help_flag("-h,--help", "显示帮助信息");

    app.add_option("--size", args.tensor_size, "张量大小")
        ->default_val(100);

    app.add_option("--input", args.input_file, "输入文件路径");

    app.add_option("--output", args.output_file, "输出文件路径");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
    exit(app.exit(e));
}

    return args;
}


} // namespace edgequant