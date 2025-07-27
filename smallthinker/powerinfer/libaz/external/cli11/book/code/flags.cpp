#include "CLI/CLI.hpp"
#include <iostream>

int main(int argc, char **argv) {
    using std::cout;
    using std::endl;
    CLI::App app{"Flag example program"};

    /// [define]
    bool flag_bool;
    app.add_flag("--bool,-b", flag_bool, "This is a bool flag");

    int flag_int;
    app.add_flag("-i,--int", flag_int, "This is an int flag");

    CLI::Option *flag_plain = app.add_flag("--plain,-p", "This is a plain flag");
    /// [define]

    /// [parser]
    try {
        app.parse(argc, argv);
    } catch(const CLI::ParseError &e) {
        return app.exit(e);
    }
    /// [parser]

    /// [usage]
    cout << "The flags program" << endl;
    if(flag_bool)
        cout << "Bool flag passed" << endl;
    if(flag_int > 0)
        cout << "Flag int: " << flag_int << endl;
    if(*flag_plain)
        cout << "Flag plain: " << flag_plain->count() << endl;
    /// [usage]
}
