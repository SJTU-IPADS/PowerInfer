// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

using vs_t = std::vector<std::string>;

TEST_CASE_METHOD(TApp, "BasicSubcommands", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = app.add_subcommand("sub2");

    CHECK(&app == sub1->get_parent());

    CHECK(app.get_subcommand(sub1) == sub1);
    CHECK(app.get_subcommand("sub1") == sub1);
    CHECK(app.get_subcommand_no_throw("sub1") == sub1);
    CHECK_THROWS_AS(app.get_subcommand("sub3"), CLI::OptionNotFound);
    CHECK_NOTHROW(app.get_subcommand_no_throw("sub3"));
    CHECK(app.get_subcommand_no_throw("sub3") == nullptr);
    run();
    CHECK(app.get_subcommands().empty());

    args = {"sub1"};
    run();
    CHECK(app.get_subcommands().at(0) == sub1);
    CHECK(app.get_subcommands().size() == 1u);

    app.clear();
    CHECK(app.get_subcommands().empty());

    args = {"sub2"};
    run();
    CHECK(app.get_subcommands().size() == 1u);
    CHECK(app.get_subcommands().at(0) == sub2);

    args = {"SUb2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"SUb2"};
    try {
        run();
    } catch(const CLI::ExtrasError &e) {
        CHECK_THAT(e.what(), Contains("SUb2"));
    }

    args = {"sub1", "extra"};
    try {
        run();
    } catch(const CLI::ExtrasError &e) {
        CHECK_THAT(e.what(), Contains("extra"));
    }
}

TEST_CASE_METHOD(TApp, "MultiSubFallthrough", "[subcom]") {

    // No explicit fallthrough
    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = app.add_subcommand("sub2");

    args = {"sub1", "sub2"};
    run();
    CHECK(app.got_subcommand("sub1"));
    CHECK(app.got_subcommand(sub1));
    CHECK(*sub1);
    CHECK(sub1->parsed());
    CHECK(1u == sub1->count());

    CHECK(app.got_subcommand("sub2"));
    CHECK(app.got_subcommand(sub2));
    CHECK(*sub2);

    app.require_subcommand();
    run();

    app.require_subcommand(2);
    run();

    app.require_subcommand(1);
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"sub1"};
    run();

    CHECK(app.got_subcommand("sub1"));
    CHECK(!app.got_subcommand("sub2"));

    CHECK(*sub1);
    CHECK(!*sub2);
    CHECK(!sub2->parsed());
    CHECK(0u == sub2->count());

    CHECK(!app.got_subcommand("sub3"));
}

TEST_CASE_METHOD(TApp, "CrazyNameSubcommand", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    // name can be set to whatever
    CHECK_NOTHROW(sub1->name("crazy name with spaces"));
    args = {"crazy name with spaces"};
    run();

    CHECK(app.got_subcommand("crazy name with spaces"));
    CHECK(1u == sub1->count());
}

TEST_CASE_METHOD(TApp, "RequiredAndSubcommands", "[subcom]") {

    std::string baz;
    app.add_option("baz", baz, "Baz Description")->required()->capture_default_str();
    auto *foo = app.add_subcommand("foo");
    auto *bar = app.add_subcommand("bar");

    args = {"bar", "foo"};
    REQUIRE_NOTHROW(run());
    CHECK(*foo);
    CHECK(!*bar);
    CHECK("bar" == baz);

    args = {"foo"};
    REQUIRE_NOTHROW(run());
    CHECK(!*foo);
    CHECK("foo" == baz);

    args = {"foo", "foo"};
    REQUIRE_NOTHROW(run());
    CHECK(*foo);
    CHECK("foo" == baz);

    args = {"foo", "other"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "RequiredAndSubcomFallthrough", "[subcom]") {

    std::string baz;
    app.add_option("baz", baz)->required();
    app.add_subcommand("foo");
    auto *bar = app.add_subcommand("bar");
    app.fallthrough();

    args = {"other", "bar"};
    run();
    CHECK(bar);
    CHECK("other" == baz);

    args = {"bar", "other2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "FooFooProblem", "[subcom]") {

    std::string baz_str, other_str;
    auto *baz = app.add_option("baz", baz_str);
    auto *foo = app.add_subcommand("foo");
    auto *other = foo->add_option("other", other_str);

    args = {"foo", "foo"};
    run();
    CHECK(*foo);
    CHECK(!*baz);
    CHECK(*other);
    CHECK(baz_str.empty());
    CHECK("foo" == other_str);

    baz_str = "";
    other_str = "";
    baz->required();
    run();
    CHECK(*foo);
    CHECK(*baz);
    CHECK(!*other);
    CHECK("foo" == baz_str);
    CHECK(other_str.empty());
}

TEST_CASE_METHOD(TApp, "DuplicateSubcommands", "[subcom]") {

    auto *foo = app.add_subcommand("foo");

    args = {"foo", "foo"};
    run();
    CHECK(*foo);
    CHECK(2u == foo->count());

    args = {"foo", "foo", "foo"};
    run();
    CHECK(*foo);
    CHECK(3u == foo->count());

    auto subs = app.get_subcommands();
    // subcommands only get triggered once
    CHECK(subs.size() == 1U);
}

TEST_CASE_METHOD(TApp, "DuplicateSubcommandCallbacks", "[subcom]") {

    auto *foo = app.add_subcommand("foo");
    int count{0};
    foo->callback([&count]() { ++count; });
    foo->immediate_callback();
    CHECK(foo->get_immediate_callback());
    args = {"foo", "foo"};
    run();
    CHECK(2 == count);
    count = 0;
    args = {"foo", "foo", "foo"};
    run();
    CHECK(3 == count);
}

TEST_CASE_METHOD(TApp, "DuplicateSubcommandCallbacksValues", "[subcom]") {

    auto *foo = app.add_subcommand("foo");
    int val{0};
    foo->add_option("--val", val);
    std::vector<int> vals;
    foo->callback([&vals, &val]() { vals.push_back(val); });
    foo->immediate_callback();
    args = {"foo", "--val=45", "foo", "--val=27"};
    run();
    CHECK(2u == vals.size());
    CHECK(45 == vals[0]);
    CHECK(27 == vals[1]);
    vals.clear();
    args = {"foo", "--val=45", "foo", "--val=27", "foo", "--val=36"};
    run();
    CHECK(3u == vals.size());
    CHECK(45 == vals[0]);
    CHECK(27 == vals[1]);
    CHECK(36 == vals[2]);
}

TEST_CASE_METHOD(TApp, "Callbacks", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    sub1->callback([]() { throw CLI::Success(); });
    auto *sub2 = app.add_subcommand("sub2");
    bool val{false};
    sub2->callback([&val]() { val = true; });

    args = {"sub2"};
    CHECK(!val);
    run();
    CHECK(val);
}

TEST_CASE_METHOD(TApp, "CallbackOrder", "[subcom]") {

    std::vector<std::string> cb;
    app.parse_complete_callback([&cb]() { cb.emplace_back("ac1"); });
    app.final_callback([&cb]() { cb.emplace_back("ac2"); });
    auto *sub1 =
        app.add_subcommand("sub1")
            ->parse_complete_callback([&cb]() { cb.emplace_back("c1"); })
            ->preparse_callback([&cb](std::size_t v1) { cb.push_back(std::string("pc1-") + std::to_string(v1)); });
    auto *sub2 =
        app.add_subcommand("sub2")
            ->final_callback([&cb]() { cb.emplace_back("c2"); })
            ->preparse_callback([&cb](std::size_t v1) { cb.push_back(std::string("pc2-") + std::to_string(v1)); });
    app.preparse_callback([&cb](std::size_t v1) { cb.push_back(std::string("pa-") + std::to_string(v1)); });

    app.add_option("--opt1");
    sub1->add_flag("--sub1opt");
    sub1->add_option("--sub1optb");
    sub1->add_flag("--sub1opt2");
    sub2->add_flag("--sub2opt");
    sub2->add_option("--sub2opt2");
    args = {"--opt1",
            "opt1_val",
            "sub1",
            "--sub1opt",
            "--sub1optb",
            "val",
            "sub2",
            "--sub2opt",
            "sub1",
            "--sub1opt2",
            "sub2",
            "--sub2opt2",
            "val"};
    run();
    CHECK(8u == cb.size());
    CHECK("pa-13" == cb[0]);
    CHECK("pc1-10" == cb[1]);
    CHECK("c1" == cb[2]);
    CHECK("pc2-6" == cb[3]);
    CHECK("c1" == cb[4]);
    CHECK("ac1" == cb[5]);
    CHECK("c2" == cb[6]);
    CHECK("ac2" == cb[7]);
}

TEST_CASE_METHOD(TApp, "CallbackOrder2", "[subcom]") {

    std::vector<std::string> cb;
    app.add_subcommand("sub1")->parse_complete_callback([&cb]() { cb.emplace_back("sub1"); });
    app.add_subcommand("sub2")->parse_complete_callback([&cb]() { cb.emplace_back("sub2"); });
    app.add_subcommand("sub3")->parse_complete_callback([&cb]() { cb.emplace_back("sub3"); });

    args = {"sub1", "sub2", "sub3", "sub1", "sub1", "sub2", "sub1"};
    run();
    CHECK(7u == cb.size());
    CHECK("sub1" == cb[0]);
    CHECK("sub2" == cb[1]);
    CHECK("sub3" == cb[2]);
    CHECK("sub1" == cb[3]);
    CHECK("sub1" == cb[4]);
    CHECK("sub2" == cb[5]);
    CHECK("sub1" == cb[6]);
}

TEST_CASE_METHOD(TApp, "CallbackOrder2_withFallthrough", "[subcom]") {

    std::vector<std::string> cb;

    app.add_subcommand("sub1")->parse_complete_callback([&cb]() { cb.emplace_back("sub1"); })->fallthrough();
    app.add_subcommand("sub2")->parse_complete_callback([&cb]() { cb.emplace_back("sub2"); });
    app.add_subcommand("sub3")->parse_complete_callback([&cb]() { cb.emplace_back("sub3"); });

    args = {"sub1", "sub2", "sub3", "sub1", "sub1", "sub2", "sub1"};
    run();
    CHECK(7u == cb.size());
    CHECK("sub1" == cb[0]);
    CHECK("sub2" == cb[1]);
    CHECK("sub3" == cb[2]);
    CHECK("sub1" == cb[3]);
    CHECK("sub1" == cb[4]);
    CHECK("sub2" == cb[5]);
    CHECK("sub1" == cb[6]);
}

TEST_CASE_METHOD(TApp, "RuntimeErrorInCallback", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    sub1->callback([]() { throw CLI::RuntimeError(); });
    auto *sub2 = app.add_subcommand("sub2");
    sub2->callback([]() { throw CLI::RuntimeError(2); });

    args = {"sub1"};
    CHECK_THROWS_AS(run(), CLI::RuntimeError);

    args = {"sub1"};
    try {
        run();
    } catch(const CLI::RuntimeError &e) {
        CHECK(e.get_exit_code() == 1);
    }

    args = {"sub2"};
    CHECK_THROWS_AS(run(), CLI::RuntimeError);

    args = {"sub2"};
    try {
        run();
    } catch(const CLI::RuntimeError &e) {
        CHECK(e.get_exit_code() == 2);
    }
}

TEST_CASE_METHOD(TApp, "NoFallThroughOpts", "[subcom]") {
    int val{1};
    app.add_option("--val", val);

    app.add_subcommand("sub");

    args = {"sub", "--val", "2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "NoFallThroughPositionals", "[subcom]") {
    int val{1};
    app.add_option("val", val);

    app.add_subcommand("sub");

    args = {"sub", "2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "NoFallThroughOptsWithTerminator", "[subcom]") {
    int val{1};
    app.add_option("--val", val);

    app.add_subcommand("sub");

    args = {"sub", "++", "--val", "2"};
    run();
    CHECK(2 == val);
}

TEST_CASE_METHOD(TApp, "NoFallThroughPositionalsWithTerminator", "[subcom]") {
    int val{1};
    app.add_option("val", val);

    app.add_subcommand("sub");

    args = {"sub", "++", "2"};
    run();
    CHECK(2 == val);

    // try with positional only mark
    args = {"sub", "--", "3"};
    run();
    CHECK(3 == val);
}

TEST_CASE_METHOD(TApp, "NamelessSubComPositionals", "[subcom]") {

    auto *sub = app.add_subcommand();
    int val{1};
    sub->add_option("val", val);

    args = {"2"};
    run();
    CHECK(2 == val);
}

TEST_CASE_METHOD(TApp, "NamelessSubWithSub", "[subcom]") {

    auto *sub = app.add_subcommand();
    auto *subsub = sub->add_subcommand("val");

    args = {"val"};
    run();
    CHECK(subsub->parsed());
    CHECK(app.got_subcommand("val"));
}

TEST_CASE_METHOD(TApp, "NamelessSubWithMultipleSub", "[subcom]") {

    auto *sub1 = app.add_subcommand();
    auto *sub2 = app.add_subcommand();
    auto *sub1sub1 = sub1->add_subcommand("val1");
    auto *sub1sub2 = sub1->add_subcommand("val2");
    auto *sub2sub1 = sub2->add_subcommand("val3");
    auto *sub2sub2 = sub2->add_subcommand("val4");
    args = {"val1"};
    run();
    CHECK(sub1sub1->parsed());
    CHECK(app.got_subcommand("val1"));

    args = {"val2"};
    run();
    CHECK(sub1sub2->parsed());
    CHECK(app.got_subcommand("val2"));

    args = {"val3"};
    run();
    CHECK(sub2sub1->parsed());
    CHECK(app.got_subcommand("val3"));

    args = {"val4"};
    run();
    CHECK(sub2sub2->parsed());
    CHECK(app.got_subcommand("val4"));

    args = {"val4", "val1"};
    run();
    CHECK(sub2sub2->parsed());
    CHECK(app.got_subcommand("val4"));
    CHECK(sub1sub1->parsed());
    CHECK(app.got_subcommand("val1"));
}

TEST_CASE_METHOD(TApp, "Nameless4LayerDeep", "[subcom]") {

    auto *sub = app.add_subcommand();
    auto *ssub = sub->add_subcommand();
    auto *sssub = ssub->add_subcommand();

    auto *ssssub = sssub->add_subcommand();
    auto *sssssub = ssssub->add_subcommand("val");

    args = {"val"};
    run();
    CHECK(sssssub->parsed());
    CHECK(app.got_subcommand("val"));
}

/// Put subcommands in some crazy pattern and make everything still works
TEST_CASE_METHOD(TApp, "Nameless4LayerDeepMulti", "[subcom]") {

    auto *sub1 = app.add_subcommand();
    auto *sub2 = app.add_subcommand();
    auto *ssub1 = sub1->add_subcommand();
    auto *ssub2 = sub2->add_subcommand();

    auto *sssub1 = ssub1->add_subcommand();
    auto *sssub2 = ssub2->add_subcommand();
    sssub1->add_subcommand("val1");
    ssub2->add_subcommand("val2");
    sub2->add_subcommand("val3");
    ssub1->add_subcommand("val4");
    sssub2->add_subcommand("val5");
    args = {"val1"};
    run();
    CHECK(app.got_subcommand("val1"));

    args = {"val2"};
    run();
    CHECK(app.got_subcommand("val2"));

    args = {"val3"};
    run();
    CHECK(app.got_subcommand("val3"));

    args = {"val4"};
    run();
    CHECK(app.got_subcommand("val4"));
    args = {"val5"};
    run();
    CHECK(app.got_subcommand("val5"));

    args = {"val4", "val1", "val5"};
    run();
    CHECK(app.got_subcommand("val4"));
    CHECK(app.got_subcommand("val1"));
    CHECK(app.got_subcommand("val5"));
}

TEST_CASE_METHOD(TApp, "FallThroughRegular", "[subcom]") {
    app.fallthrough();
    int val{1};
    app.add_option("--val", val);

    app.add_subcommand("sub");

    args = {"sub", "--val", "2"};
    // Should not throw
    run();
}

TEST_CASE_METHOD(TApp, "FallThroughShort", "[subcom]") {
    app.fallthrough();
    int val{1};
    app.add_option("-v", val);

    app.add_subcommand("sub");

    args = {"sub", "-v", "2"};
    // Should not throw
    run();
}

TEST_CASE_METHOD(TApp, "FallThroughPositional", "[subcom]") {
    app.fallthrough();
    int val{1};
    app.add_option("val", val);

    app.add_subcommand("sub");

    args = {"sub", "2"};
    // Should not throw
    run();
}

TEST_CASE_METHOD(TApp, "FallThroughEquals", "[subcom]") {
    app.fallthrough();
    int val{1};
    app.add_option("--val", val);

    app.add_subcommand("sub");

    args = {"sub", "--val=2"};
    // Should not throw
    run();
}

TEST_CASE_METHOD(TApp, "EvilParseFallthrough", "[subcom]") {
    app.fallthrough();
    int val1{0}, val2{0};
    app.add_option("--val1", val1);

    auto *sub = app.add_subcommand("sub");
    sub->add_option("val2", val2);

    args = {"sub", "--val1", "1", "2"};
    // Should not throw
    run();

    CHECK(val1 == 1);
    CHECK(val2 == 2);
}

TEST_CASE_METHOD(TApp, "CallbackOrdering", "[subcom]") {
    app.fallthrough();
    int val{1}, sub_val{0};
    app.add_option("--val", val);

    auto *sub = app.add_subcommand("sub");
    sub->callback([&val, &sub_val]() { sub_val = val; });

    args = {"sub", "--val=2"};
    run();
    CHECK(val == 2);
    CHECK(sub_val == 2);

    args = {"--val=2", "sub"};
    run();
    CHECK(val == 2);
    CHECK(sub_val == 2);
}

TEST_CASE_METHOD(TApp, "CallbackOrderingImmediate", "[subcom]") {
    app.fallthrough();
    int val{1}, sub_val{0};
    app.add_option("--val", val);

    auto *sub = app.add_subcommand("sub")->immediate_callback();
    sub->callback([&val, &sub_val]() { sub_val = val; });

    args = {"sub", "--val=2"};
    run();
    CHECK(val == 2);
    CHECK(sub_val == 1);

    args = {"--val=2", "sub"};
    run();
    CHECK(val == 2);
    CHECK(sub_val == 2);
}

TEST_CASE_METHOD(TApp, "CallbackOrderingImmediateMain", "[subcom]") {
    app.fallthrough();
    int val{0}, sub_val{0};

    auto *sub = app.add_subcommand("sub");
    sub->callback([&val, &sub_val]() {
        sub_val = val;
        val = 2;
    });
    app.callback([&val]() { val = 1; });
    args = {"sub"};
    run();
    CHECK(val == 1);
    CHECK(sub_val == 0);
    // the main app callback should run before the subcommand callbacks
    app.immediate_callback();
    val = 0;  // reset value
    run();
    CHECK(val == 2);
    CHECK(sub_val == 1);
    // the subcommand callback now runs immediately after processing and before the main app callback again
    sub->immediate_callback();
    val = 0;  // reset value
    run();
    CHECK(val == 1);
    CHECK(sub_val == 0);
}

// Test based on issue #308
TEST_CASE_METHOD(TApp, "CallbackOrderingImmediateModeOrder", "[subcom]") {

    app.require_subcommand(1, 1);
    std::vector<int> v;
    app.callback([&v]() { v.push_back(1); })->immediate_callback(true);

    auto *sub = app.add_subcommand("hello")->callback([&v]() { v.push_back(2); })->immediate_callback(false);
    args = {"hello"};
    run();
    // immediate_callback inherited
    REQUIRE(2u == v.size());
    CHECK(1 == v[0]);
    CHECK(2 == v[1]);
    v.clear();
    sub->immediate_callback(true);
    run();
    // immediate_callback is now triggered for the main first
    REQUIRE(2u == v.size());
    CHECK(2 == v[0]);
    CHECK(1 == v[1]);
}

TEST_CASE_METHOD(TApp, "RequiredSubCom", "[subcom]") {
    app.add_subcommand("sub1");
    app.add_subcommand("sub2");

    app.require_subcommand();

    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"sub1"};
    run();
}

TEST_CASE_METHOD(TApp, "SubComExtras", "[subcom]") {
    app.allow_extras();
    auto *sub = app.add_subcommand("sub");

    args = {"extra", "sub"};
    run();
    CHECK(std::vector<std::string>({"extra"}) == app.remaining());
    CHECK(sub->remaining().empty());

    args = {"extra1", "extra2", "sub"};
    run();
    CHECK(std::vector<std::string>({"extra1", "extra2"}) == app.remaining());
    CHECK(sub->remaining().empty());

    args = {"sub", "extra1", "extra2"};
    run();
    CHECK(app.remaining().empty());
    CHECK(std::vector<std::string>({"extra1", "extra2"}) == sub->remaining());

    args = {"extra1", "extra2", "sub", "extra3", "extra4"};
    run();
    CHECK(std::vector<std::string>({"extra1", "extra2"}) == app.remaining());
    CHECK(std::vector<std::string>({"extra1", "extra2", "extra3", "extra4"}) == app.remaining(true));
    CHECK(std::vector<std::string>({"extra3", "extra4"}) == sub->remaining());
}

TEST_CASE_METHOD(TApp, "Required1SubCom", "[subcom]") {
    app.require_subcommand(1);
    app.add_subcommand("sub1");
    app.add_subcommand("sub2");
    app.add_subcommand("sub3");

    CHECK_THROWS_AS(run(), CLI::RequiredError);

    args = {"sub1"};
    CHECK_NOTHROW(run());

    args = {"sub1", "sub2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "subcomNoSubComfallthrough", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    std::vector<std::string> pos;
    sub1->add_option("args", pos);
    app.add_subcommand("sub2");
    app.add_subcommand("sub3");
    sub1->subcommand_fallthrough(false);
    CHECK_FALSE(sub1->get_subcommand_fallthrough());
    args = {"sub1", "sub2", "sub3"};
    run();
    CHECK(pos.size() == 2);
}

TEST_CASE_METHOD(TApp, "BadSubcommandSearch", "[subcom]") {

    auto *one = app.add_subcommand("one");
    auto *two = one->add_subcommand("two");

    CHECK_THROWS_AS(app.get_subcommand(two), CLI::OptionNotFound);
    CHECK_THROWS_AS(app.get_subcommand_ptr(two), CLI::OptionNotFound);
}

TEST_CASE_METHOD(TApp, "PrefixProgram", "[subcom]") {

    app.prefix_command();

    app.add_flag("--simple");

    args = {"--simple", "other", "--simple", "--mine"};
    run();

    CHECK(std::vector<std::string>({"other", "--simple", "--mine"}) == app.remaining());
}

TEST_CASE_METHOD(TApp, "PrefixNoSeparation", "[subcom]") {

    app.prefix_command();

    std::vector<int> vals;
    app.add_option("--vals", vals);

    args = {"--vals", "1", "2", "3", "other"};

    CHECK_THROWS_AS(run(), CLI::ConversionError);
}

TEST_CASE_METHOD(TApp, "PrefixSeparation", "[subcom]") {

    app.prefix_command();

    std::vector<int> vals;
    app.add_option("--vals", vals);

    args = {"--vals", "1", "2", "3", "--", "other"};

    run();

    CHECK(std::vector<std::string>({"other"}) == app.remaining());
    CHECK(std::vector<int>({1, 2, 3}) == vals);
}

TEST_CASE_METHOD(TApp, "PrefixSubcom", "[subcom]") {
    auto *subc = app.add_subcommand("subc");
    subc->prefix_command();

    app.add_flag("--simple");

    args = {"--simple", "subc", "other", "--simple", "--mine"};
    run();

    CHECK(0u == app.remaining_size());
    CHECK(3u == app.remaining_size(true));
    CHECK(std::vector<std::string>({"other", "--simple", "--mine"}) == subc->remaining());
}

TEST_CASE_METHOD(TApp, "InheritHelpAllFlag", "[subcom]") {
    app.set_help_all_flag("--help-all");
    auto *subc = app.add_subcommand("subc");
    auto help_opt_list = subc->get_options([](const CLI::Option *opt) { return opt->get_name() == "--help-all"; });
    CHECK(1u == help_opt_list.size());
}

TEST_CASE_METHOD(TApp, "RequiredPosInSubcommand", "[subcom]") {
    app.require_subcommand();
    std::string bar;

    CLI::App *fooApp = app.add_subcommand("foo", "Foo a bar");
    fooApp->add_option("bar", bar, "A bar to foo")->required();

    CLI::App *bazApp = app.add_subcommand("baz", "Baz a bar");
    bazApp->add_option("bar", bar, "A bar a baz")->required();

    args = {"foo", "abc"};
    run();
    CHECK("abc" == bar);
    args = {"baz", "cba"};
    run();
    CHECK("cba" == bar);

    args = {};
    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

// from  https://github.com/CLIUtils/CLI11/issues/1002
TEST_CASE_METHOD(TApp, "ForcedSubcommandExclude", "[subcom]") {
    auto *subcommand_1 = app.add_subcommand("sub_1");
    std::string forced;
    subcommand_1->add_flag_function("-f", [&forced](bool f) { forced = f ? "got true" : "got false"; })
        ->force_callback();

    auto *subcommand_2 = app.add_subcommand("sub2");

    subcommand_1->excludes(subcommand_2);

    args = {"sub2"};
    CHECK_NOTHROW(run());
    CHECK(forced == "got false");
}

TEST_CASE_METHOD(TApp, "invalidSubcommandName", "[subcom]") {

    bool gotError{false};
    try {
        app.add_subcommand("!foo/foo", "Foo a bar");
    } catch(const CLI::IncorrectConstruction &e) {
        gotError = true;
        CHECK_THAT(e.what(), Contains("!"));
    }
    CHECK(gotError);
}

struct SubcommandProgram : public TApp {

    CLI::App *start{nullptr};
    CLI::App *stop{nullptr};

    int dummy{0};
    std::string file{};
    int count{0};

    SubcommandProgram(const SubcommandProgram &) = delete;
    SubcommandProgram &operator=(const SubcommandProgram &) = delete;

    SubcommandProgram() {
        app.set_help_all_flag("--help-all");

        start = app.add_subcommand("start", "Start prog");
        stop = app.add_subcommand("stop", "Stop prog");

        app.add_flag("-d", dummy, "My dummy var");
        start->add_option("-f,--file", file, "File name");
        stop->add_flag("-c,--count", count, "Some flag opt");
    }
};

TEST_CASE_METHOD(SubcommandProgram, "Subcommand Working", "[subcom]") {
    args = {"-d", "start", "-ffilename"};

    run();

    CHECK(dummy == 1);
    CHECK(app.get_subcommands().at(0) == start);
    CHECK(file == "filename");
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand Spare", "[subcom]") {
    args = {"extra", "-d", "start", "-ffilename"};

    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand SpareSub", "[subcom]") {
    args = {"-d", "start", "spare", "-ffilename"};

    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand Multiple", "[subcom]") {
    args = {"-d", "start", "-ffilename", "stop"};

    run();
    CHECK(app.get_subcommands().size() == 2u);
    CHECK(dummy == 1);
    CHECK(file == "filename");
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand MultipleOtherOrder", "[subcom]") {
    args = {"start", "-d", "-ffilename", "stop"};

    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand MultipleArgs", "[subcom]") {
    args = {"start", "stop"};

    run();

    CHECK(app.get_subcommands().size() == 2u);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand CaseCheck", "[subcom]") {
    args = {"Start"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"start"};
    run();

    start->ignore_case();
    run();

    args = {"Start"};
    run();
}

TEST_CASE_METHOD(TApp, "SubcomInheritCaseCheck", "[subcom]") {
    app.ignore_case();
    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = app.add_subcommand("sub2");

    run();
    CHECK(app.get_subcommands().empty());
    CHECK(app.get_subcommands({}).size() == 2u);
    CHECK(app.get_subcommands([](const CLI::App *s) { return s->get_name() == "sub1"; }).size() == 1u);

    // check the const version of get_subcommands
    const auto &app_const = app;
    CHECK(app_const.get_subcommands([](const CLI::App *s) { return s->get_name() == "sub1"; }).size() == 1u);

    args = {"SuB1"};
    run();
    CHECK(app.get_subcommands().at(0) == sub1);
    CHECK(app.get_subcommands().size() == 1u);

    app.clear();
    CHECK(app.get_subcommands().empty());

    args = {"sUb2"};
    run();
    CHECK(app.get_subcommands().at(0) == sub2);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand UnderscoreCheck", "[subcom]") {
    args = {"start_"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"start"};
    run();

    start->ignore_underscore();
    run();

    args = {"_start_"};
    run();
}

TEST_CASE_METHOD(TApp, "SubcomInheritUnderscoreCheck", "[subcom]") {
    app.ignore_underscore();
    auto *sub1 = app.add_subcommand("sub_option1");
    auto *sub2 = app.add_subcommand("sub_option2");

    run();
    CHECK(app.get_subcommands().empty());
    CHECK(app.get_subcommands({}).size() == 2u);
    CHECK(app.get_subcommands([](const CLI::App *s) { return s->get_name() == "sub_option1"; }).size() == 1u);

    args = {"suboption1"};
    run();
    CHECK(app.get_subcommands().at(0) == sub1);
    CHECK(app.get_subcommands().size() == 1u);

    app.clear();
    CHECK(app.get_subcommands().empty());

    args = {"_suboption2"};
    run();
    CHECK(app.get_subcommands().at(0) == sub2);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand HelpOrder", "[subcom]") {

    args = {"-h"};
    CHECK_THROWS_AS(run(), CLI::CallForHelp);

    args = {"start", "-h"};
    CHECK_THROWS_AS(run(), CLI::CallForHelp);

    args = {"-h", "start"};
    CHECK_THROWS_AS(run(), CLI::CallForHelp);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand HelpAllOrder", "[subcom]") {

    args = {"--help-all"};
    CHECK_THROWS_AS(run(), CLI::CallForAllHelp);

    args = {"start", "--help-all"};
    CHECK_THROWS_AS(run(), CLI::CallForAllHelp);

    args = {"--help-all", "start"};
    CHECK_THROWS_AS(run(), CLI::CallForAllHelp);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand Callbacks", "[subcom]") {

    start->callback([]() { throw CLI::Success(); });

    run();

    args = {"start"};

    CHECK_THROWS_AS(run(), CLI::Success);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand Groups", "[subcom]") {

    std::string help = app.help();
    CHECK_THAT(help, !Contains("More Commands:"));
    CHECK_THAT(help, Contains("SUBCOMMANDS:"));

    start->group("More Commands");
    help = app.help();
    CHECK_THAT(help, Contains("More Commands:"));
    CHECK_THAT(help, Contains("SUBCOMMANDS:"));

    // Case is ignored but for the first subcommand in a group.
    stop->group("more commands");
    help = app.help();
    CHECK_THAT(help, Contains("More Commands:"));
    CHECK_THAT(help, !Contains("SUBCOMMANDS:"));
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand ExtrasErrors", "[subcom]") {

    args = {"one", "two", "start", "three", "four"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"start", "three", "four"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"one", "two"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand OrderedExtras", "[subcom]") {

    app.allow_extras();
    args = {"one", "two", "start", "three", "four"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    start->allow_extras();

    run();

    CHECK(std::vector<std::string>({"one", "two"}) == app.remaining());
    CHECK(std::vector<std::string>({"three", "four"}) == start->remaining());
    CHECK(std::vector<std::string>({"one", "two", "three", "four"}) == app.remaining(true));

    args = {"one", "two", "start", "three", "--", "four"};

    run();

    CHECK(std::vector<std::string>({"one", "two", "four"}) == app.remaining());
    CHECK(std::vector<std::string>({"three"}) == start->remaining());
    CHECK(std::vector<std::string>({"one", "two", "four", "three"}) == app.remaining(true));
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand MixedOrderExtras", "[subcom]") {

    app.allow_extras();
    start->allow_extras();
    stop->allow_extras();

    args = {"one", "two", "start", "three", "four", "stop", "five", "six"};
    run();

    CHECK(std::vector<std::string>({"one", "two"}) == app.remaining());
    CHECK(std::vector<std::string>({"three", "four"}) == start->remaining());
    CHECK(std::vector<std::string>({"five", "six"}) == stop->remaining());
    CHECK(std::vector<std::string>({"one", "two", "three", "four", "five", "six"}) == app.remaining(true));

    args = {"one", "two", "stop", "three", "four", "start", "five", "six"};
    run();

    CHECK(std::vector<std::string>({"one", "two"}) == app.remaining());
    CHECK(std::vector<std::string>({"three", "four"}) == stop->remaining());
    CHECK(std::vector<std::string>({"five", "six"}) == start->remaining());
    CHECK(std::vector<std::string>({"one", "two", "three", "four", "five", "six"}) == app.remaining(true));
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand CallbackOrder", "[subcom]") {
    std::vector<int> callback_order;
    start->callback([&callback_order]() { callback_order.push_back(1); });
    stop->callback([&callback_order]() { callback_order.push_back(2); });

    args = {"start", "stop"};
    run();
    CHECK(std::vector<int>({1, 2}) == callback_order);

    callback_order.clear();

    args = {"stop", "start"};
    run();
    CHECK(std::vector<int>({2, 1}) == callback_order);
}

TEST_CASE_METHOD(SubcommandProgram, "Subcommand CallbackOrderImmediate", "[subcom]") {
    std::vector<int> callback_order;
    start->callback([&callback_order]() { callback_order.push_back(1); })->immediate_callback();
    stop->callback([&callback_order]() { callback_order.push_back(2); });

    args = {"start", "stop", "start"};
    run();
    CHECK(std::vector<int>({1, 1, 2}) == callback_order);

    callback_order.clear();

    args = {"stop", "start", "stop", "start"};
    run();
    CHECK(std::vector<int>({1, 1, 2}) == callback_order);
}

struct ManySubcommands : public TApp {

    CLI::App *sub1{nullptr};
    CLI::App *sub2{nullptr};
    CLI::App *sub3{nullptr};
    CLI::App *sub4{nullptr};

    ManySubcommands() {
        app.allow_extras();
        sub1 = app.add_subcommand("sub1");
        sub2 = app.add_subcommand("sub2");
        sub3 = app.add_subcommand("sub3");
        sub4 = app.add_subcommand("sub4");
        args = {"sub1", "sub2", "sub3"};
    }

    ManySubcommands(const ManySubcommands &) = delete;
    ManySubcommands &operator=(const ManySubcommands &) = delete;
};

TEST_CASE_METHOD(ManySubcommands, "Required1Exact", "[subcom]") {
    app.require_subcommand(1);

    run();
    CHECK(vs_t({"sub2", "sub3"}) == sub1->remaining());
    CHECK(vs_t({"sub2", "sub3"}) == app.remaining(true));
}

TEST_CASE_METHOD(ManySubcommands, "Required2Exact", "[subcom]") {
    app.require_subcommand(2);

    run();
    CHECK(vs_t({"sub3"}) == sub2->remaining());
}

TEST_CASE_METHOD(ManySubcommands, "Required4Failure", "[subcom]") {
    app.require_subcommand(4);

    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(ManySubcommands, "RemoveSub", "[subcom]") {
    run();
    CHECK(0u == app.remaining_size(true));
    app.remove_subcommand(sub1);
    app.allow_extras();
    run();
    CHECK(1u == app.remaining_size(true));
}

TEST_CASE_METHOD(ManySubcommands, "RemoveSubFail", "[subcom]") {
    auto *sub_sub = sub1->add_subcommand("subsub");
    CHECK(!app.remove_subcommand(sub_sub));
    CHECK(sub1->remove_subcommand(sub_sub));
    CHECK(!app.remove_subcommand(nullptr));
}

TEST_CASE_METHOD(ManySubcommands, "manyIndexQuery", "[subcom]") {
    auto *s1 = app.get_subcommand(0);
    auto *s2 = app.get_subcommand(1);
    auto *s3 = app.get_subcommand(2);
    auto *s4 = app.get_subcommand(3);
    CHECK(sub1 == s1);
    CHECK(sub2 == s2);
    CHECK(sub3 == s3);
    CHECK(sub4 == s4);
    CHECK_THROWS_AS(app.get_subcommand(4), CLI::OptionNotFound);
    auto *s0 = app.get_subcommand();
    CHECK(sub1 == s0);
}

TEST_CASE_METHOD(ManySubcommands, "manyIndexQueryPtr", "[subcom]") {
    auto s1 = app.get_subcommand_ptr(0);
    auto s2 = app.get_subcommand_ptr(1);
    auto s3 = app.get_subcommand_ptr(2);
    auto s4 = app.get_subcommand_ptr(3);
    CHECK(sub1 == s1.get());
    CHECK(sub2 == s2.get());
    CHECK(sub3 == s3.get());
    CHECK(sub4 == s4.get());
    CHECK_THROWS_AS(app.get_subcommand_ptr(4), CLI::OptionNotFound);
}

TEST_CASE_METHOD(ManySubcommands, "manyIndexQueryPtrByName", "[subcom]") {
    auto s1 = app.get_subcommand_ptr("sub1");
    auto s2 = app.get_subcommand_ptr("sub2");
    auto s3 = app.get_subcommand_ptr("sub3");
    auto s4 = app.get_subcommand_ptr("sub4");
    CHECK(sub1 == s1.get());
    CHECK(sub2 == s2.get());
    CHECK(sub3 == s3.get());
    CHECK(sub4 == s4.get());
    CHECK_THROWS_AS(app.get_subcommand_ptr("sub5"), CLI::OptionNotFound);
}

TEST_CASE_METHOD(ManySubcommands, "Required1Fuzzy", "[subcom]") {

    app.require_subcommand(0, 1);

    run();
    CHECK(vs_t({"sub2", "sub3"}) == sub1->remaining());

    app.require_subcommand(-1);

    run();
    CHECK(vs_t({"sub2", "sub3"}) == sub1->remaining());
}

TEST_CASE_METHOD(ManySubcommands, "Required2Fuzzy", "[subcom]") {
    app.require_subcommand(0, 2);

    run();
    CHECK(vs_t({"sub3"}) == sub2->remaining());
    CHECK(vs_t({"sub3"}) == app.remaining(true));

    app.require_subcommand(-2);

    run();
    CHECK(vs_t({"sub3"}) == sub2->remaining());
}

TEST_CASE_METHOD(ManySubcommands, "Unlimited", "[subcom]") {
    run();
    CHECK(app.remaining(true).empty());

    app.require_subcommand();

    run();
    CHECK(app.remaining(true).empty());

    app.require_subcommand(2, 0);  // 2 or more

    run();
    CHECK(app.remaining(true).empty());
}

TEST_CASE_METHOD(ManySubcommands, "HelpFlags", "[subcom]") {

    args = {"-h"};

    CHECK_THROWS_AS(run(), CLI::CallForHelp);

    args = {"sub2", "-h"};

    CHECK_THROWS_AS(run(), CLI::CallForHelp);

    args = {"-h", "sub2"};

    CHECK_THROWS_AS(run(), CLI::CallForHelp);
}

TEST_CASE_METHOD(ManySubcommands, "MaxCommands", "[subcom]") {

    app.require_subcommand(2);

    args = {"sub1", "sub2"};
    CHECK_NOTHROW(run());

    // The extra subcommand counts as an extra
    args = {"sub1", "sub2", "sub3"};
    CHECK_NOTHROW(run());
    CHECK(1u == sub2->remaining().size());
    CHECK(2u == app.count_all());

    // Currently, setting sub2 to throw causes an extras error
    // In the future, would passing on up to app's extras be better?

    app.allow_extras(false);
    sub1->allow_extras(false);
    sub2->allow_extras(false);

    args = {"sub1", "sub2"};

    CHECK_NOTHROW(run());

    args = {"sub1", "sub2", "sub3"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandExclusion", "[subcom]") {

    sub1->excludes(sub3);
    sub2->excludes(sub3);
    args = {"sub1", "sub2"};
    CHECK_NOTHROW(run());

    args = {"sub1", "sub2", "sub3"};
    CHECK_THROWS_AS(run(), CLI::ExcludesError);

    args = {"sub1", "sub2", "sub4"};
    CHECK_NOTHROW(run());
    CHECK(3u == app.count_all());

    args = {"sub3", "sub4"};
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandOptionExclusion", "[subcom]") {

    auto *excluder_flag = app.add_flag("--exclude");
    sub1->excludes(excluder_flag)->fallthrough();
    sub2->excludes(excluder_flag)->fallthrough();
    sub3->fallthrough();
    sub4->fallthrough();
    args = {"sub3", "sub4", "--exclude"};
    CHECK_NOTHROW(run());

    args = {"sub1", "sub3", "--exclude"};
    CHECK_THROWS_AS(run(), CLI::ExcludesError);
    CHECK(sub1->remove_excludes(excluder_flag));
    CHECK_NOTHROW(run());
    CHECK(!sub1->remove_excludes(excluder_flag));

    args = {"--exclude", "sub2", "sub4"};
    CHECK_THROWS_AS(run(), CLI::ExcludesError);
    CHECK(sub1 == sub1->excludes(excluder_flag));
    args = {"sub1", "--exclude", "sub2", "sub4"};
    try {
        run();
    } catch(const CLI::ExcludesError &ee) {
        CHECK(std::string::npos != std::string(ee.what()).find("sub1"));
    }
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandNeeds", "[subcom]") {

    sub1->needs(sub2);
    args = {"sub1", "sub2"};
    CHECK_NOTHROW(run());

    args = {"sub2"};
    CHECK_NOTHROW(run());

    args = {"sub1"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    sub1->needs(sub3);
    args = {"sub1", "sub2", "sub3"};
    CHECK_NOTHROW(run());

    args = {"sub1", "sub2", "sub4"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"sub1", "sub2", "sub4"};
    sub1->remove_needs(sub3);
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandNeedsOptions", "[subcom]") {

    auto *opt = app.add_flag("--subactive");
    sub1->needs(opt);
    sub1->fallthrough();
    args = {"sub1", "--subactive"};
    CHECK_NOTHROW(run());

    args = {"sub1"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--subactive"};
    CHECK_NOTHROW(run());

    auto *opt2 = app.add_flag("--subactive2");

    sub1->needs(opt2);
    args = {"sub1", "--subactive"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);

    args = {"--subactive", "--subactive2", "sub1"};
    CHECK_NOTHROW(run());

    sub1->remove_needs(opt2);
    args = {"sub1", "--subactive"};
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandNeedsOptionsCallbackOrdering", "[subcom]") {
    int count{0};
    auto *opt = app.add_flag("--subactive");
    app.add_flag("--flag1");
    sub1->needs(opt);
    sub1->fallthrough();
    sub1->parse_complete_callback([&count]() { ++count; });
    args = {"sub1", "--flag1", "sub1", "--subactive"};
    CHECK_THROWS_AS(run(), CLI::RequiresError);
    // the subcommand has to pass validation by the first callback
    sub1->immediate_callback(false);
    // now since the callback executes after

    CHECK_NOTHROW(run());
    CHECK(1 == count);
    sub1->immediate_callback();
    args = {"--subactive", "sub1"};
    // now the required is processed first
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandParseCompleteDotNotation", "[subcom]") {
    int count{0};
    sub1->add_flag("--flag1");
    sub1->parse_complete_callback([&count]() { ++count; });
    args = {"--sub1.flag1", "--sub1.flag1"};
    run();
    CHECK(count == 2);
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandNeedsFail", "[subcom]") {

    auto *opt = app.add_flag("--subactive");
    auto *opt2 = app.add_flag("--dummy");
    sub1->needs(opt);
    CHECK_THROWS_AS(sub1->needs((CLI::Option *)nullptr), CLI::OptionNotFound);
    CHECK_THROWS_AS(sub1->needs((CLI::App *)nullptr), CLI::OptionNotFound);
    CHECK_THROWS_AS(sub1->needs(sub1), CLI::OptionNotFound);

    CHECK(sub1->remove_needs(opt));
    CHECK(!sub1->remove_needs(opt2));
    CHECK(!sub1->remove_needs(sub1));
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandRequired", "[subcom]") {

    sub1->required();
    args = {"sub1", "sub2"};
    CHECK_NOTHROW(run());

    args = {"sub1", "sub2", "sub3"};
    CHECK_NOTHROW(run());

    args = {"sub3", "sub4"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandDisabled", "[subcom]") {

    sub3->disabled();
    args = {"sub1", "sub2"};
    CHECK_NOTHROW(run());

    args = {"sub1", "sub2", "sub3"};
    app.allow_extras(false);
    sub2->allow_extras(false);
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
    args = {"sub3", "sub4"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
    sub3->disabled(false);
    args = {"sub3", "sub4"};
    CHECK_NOTHROW(run());
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandTriggeredOff", "[subcom]") {

    app.allow_extras(false);
    sub1->allow_extras(false);
    sub2->allow_extras(false);
    CLI::TriggerOff(sub1, sub2);
    args = {"sub1", "sub2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"sub2", "sub1", "sub3"};
    CHECK_NOTHROW(run());
    CLI::TriggerOff(sub1, {sub3, sub4});
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
    args = {"sub1", "sub2", "sub4"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandTriggeredOn", "[subcom]") {

    app.allow_extras(false);
    sub1->allow_extras(false);
    sub2->allow_extras(false);
    CLI::TriggerOn(sub1, sub2);
    args = {"sub1", "sub2"};
    CHECK_NOTHROW(run());

    args = {"sub2", "sub1", "sub4"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
    CLI::TriggerOn(sub1, {sub3, sub4});
    sub2->disabled_by_default(false);
    sub2->disabled(false);
    CHECK_NOTHROW(run());
    args = {"sub3", "sub1", "sub2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(ManySubcommands, "SubcommandSilence", "[subcom]") {

    sub1->silent();
    args = {"sub1", "sub2"};
    CHECK_NOTHROW(run());

    auto subs = app.get_subcommands();
    CHECK(1U == subs.size());
    sub1->silent(false);
    CHECK(!sub1->get_silent());
    run();
    subs = app.get_subcommands();
    CHECK(2U == subs.size());
}

TEST_CASE_METHOD(TApp, "UnnamedSub", "[subcom]") {
    double val{0.0};
    auto *sub = app.add_subcommand("", "empty name");
    auto *opt = sub->add_option("-v,--value", val);
    args = {"-v", "4.56"};

    run();
    CHECK(4.56 == val);
    // make sure unnamed sub options can be found from the main app
    auto *opt2 = app.get_option("-v");
    CHECK(opt2 == opt);

    CHECK_THROWS_AS(app.get_option("--vvvv"), CLI::OptionNotFound);
    // now test in the constant context
    const auto &appC = app;
    const auto *opt3 = appC.get_option("-v");
    CHECK("--value" == opt3->get_name());
    CHECK_THROWS_AS(appC.get_option("--vvvv"), CLI::OptionNotFound);
}

TEST_CASE_METHOD(TApp, "UnnamedSubMix", "[subcom]") {
    double val{0.0}, val2{0.0}, val3{0.0};
    app.add_option("-t", val2);
    auto *sub1 = app.add_subcommand("", "empty name");
    sub1->add_option("-v,--value", val);
    auto *sub2 = app.add_subcommand("", "empty name2");
    sub2->add_option("-m,--mix", val3);
    args = {"-m", "4.56", "-t", "5.93", "-v", "-3"};

    run();
    CHECK(-3.0 == val);
    CHECK(5.93 == val2);
    CHECK(4.56 == val3);
    CHECK(3u == app.count_all());
}

TEST_CASE_METHOD(TApp, "UnnamedSubMixExtras", "[subcom]") {
    double val{0.0}, val2{0.0};
    app.add_option("-t", val2);
    auto *sub = app.add_subcommand("", "empty name");
    sub->add_option("-v,--value", val);
    args = {"-m", "4.56", "-t", "5.93", "-v", "-3"};
    app.allow_extras();
    run();
    CHECK(-3.0 == val);
    CHECK(5.93 == val2);
    CHECK(2u == app.remaining_size());
    CHECK(0u == sub->remaining_size());
}

TEST_CASE_METHOD(TApp, "UnnamedSubNoExtras", "[subcom]") {
    double val{0.0}, val2{0.0};
    app.add_option("-t", val2);
    auto *sub = app.add_subcommand();
    sub->add_option("-v,--value", val);
    args = {"-t", "5.93", "-v", "-3"};
    run();
    CHECK(-3.0 == val);
    CHECK(5.93 == val2);
    CHECK(0u == app.remaining_size());
    CHECK(0u == sub->remaining_size());
}

TEST_CASE_METHOD(TApp, "SubcommandAlias", "[subcom]") {
    double val{0.0};
    auto *sub = app.add_subcommand("sub1");
    sub->alias("sub2");
    sub->alias("sub3");
    sub->add_option("-v,--value", val);
    args = {"sub1", "-v", "-3"};
    run();
    CHECK(-3.0 == val);

    args = {"sub2", "--value", "-5"};
    run();
    CHECK(-5.0 == val);

    args = {"sub3", "-v", "7"};
    run();
    CHECK(7 == val);

    const auto &al = sub->get_aliases();
    REQUIRE(2U <= al.size());

    CHECK("sub2" == al[0]);
    CHECK("sub3" == al[1]);

    sub->clear_aliases();
    CHECK(al.empty());
}

TEST_CASE_METHOD(TApp, "SubcommandAliasIgnoreCaseUnderscore", "[subcom]") {
    double val{0.0};
    auto *sub = app.add_subcommand("sub1");
    sub->alias("sub2");
    sub->alias("sub3");
    sub->ignore_case();
    sub->add_option("-v,--value", val);
    args = {"sub1", "-v", "-3"};
    run();
    CHECK(-3.0 == val);

    args = {"SUB2", "--value", "-5"};
    run();
    CHECK(-5.0 == val);

    args = {"sUb3", "-v", "7"};
    run();
    CHECK(7 == val);
    sub->ignore_underscore();
    args = {"sub_1", "-v", "-3"};
    run();
    CHECK(-3.0 == val);

    args = {"SUB_2", "--value", "-5"};
    run();
    CHECK(-5.0 == val);

    args = {"sUb_3", "-v", "7"};
    run();
    CHECK(7 == val);

    sub->ignore_case(false);
    args = {"sub_1", "-v", "-3"};
    run();
    CHECK(-3.0 == val);

    args = {"SUB_2", "--value", "-5"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"sUb_3", "-v", "7"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
}

TEST_CASE_METHOD(TApp, "OptionGroupAlias", "[subcom]") {
    double val{0.0};
    auto *sub = app.add_option_group("sub1");
    sub->alias("sub2");
    sub->alias("sub3");
    sub->add_option("-v,--value", val);
    args = {"sub1", "-v", "-3"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"sub2", "--value", "-5"};
    run();
    CHECK(-5.0 == val);

    args = {"sub3", "-v", "7"};
    run();
    CHECK(7 == val);

    args = {"-v", "-3"};
    run();
    CHECK(-3 == val);
}

TEST_CASE_METHOD(TApp, "OptionGroupAliasWithSpaces", "[subcom]") {
    double val{0.0};
    auto *sub = app.add_option_group("sub1");
    sub->alias("sub2 bb");
    sub->alias("sub3/b");
    sub->add_option("-v,--value", val);
    args = {"sub1", "-v", "-3"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"sub2 bb", "--value", "-5"};
    run();
    CHECK(-5.0 == val);

    args = {"sub3/b", "-v", "7"};
    run();
    CHECK(7 == val);

    args = {"-v", "-3"};
    run();
    CHECK(-3 == val);
}

TEST_CASE_METHOD(TApp, "subcommand_help", "[subcom]") {
    auto *sub1 = app.add_subcommand("help")->silent();
    bool flag{false};
    app.add_flag("--one", flag, "FLAGGER");
    sub1->parse_complete_callback([]() { throw CLI::CallForHelp(); });
    bool called{false};
    args = {"help"};
    try {
        run();
    } catch(const CLI::CallForHelp &) {
        called = true;
    }
    auto helpstr = app.help();
    CHECK_THAT(helpstr, Contains("FLAGGER"));
    CHECK(called);
}

TEST_CASE_METHOD(TApp, "AliasErrors", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = app.add_subcommand("sub2");

    CHECK_THROWS_AS(sub2->alias("this is a not\n a valid alias"), CLI::IncorrectConstruction);
    CHECK_NOTHROW(sub2->alias("-alias"));  // this is allowed but would be unusable on command line parsers

    CHECK_THROWS_AS(app.add_subcommand("--bad_subcommand_name", "documenting the bad subcommand"),
                    CLI::IncorrectConstruction);

    CHECK_THROWS_AS(app.add_subcommand("documenting a subcommand", "sub3"), CLI::IncorrectConstruction);
    // cannot alias to an existing subcommand
    CHECK_THROWS_AS(sub2->alias("sub1"), CLI::OptionAlreadyAdded);
    CHECK_THROWS_AS(sub1->alias("sub2"), CLI::OptionAlreadyAdded);
    // aliasing to an existing name should be allowed
    CHECK_NOTHROW(sub1->alias(sub1->get_name()));

    sub1->alias("les1")->alias("les2")->alias("les_3");
    sub2->alias("s2les1")->alias("s2les2")->alias("s2les3");

    CHECK_THROWS_AS(sub2->alias("les2"), CLI::OptionAlreadyAdded);
    CHECK_THROWS_AS(sub1->alias("s2les2"), CLI::OptionAlreadyAdded);

    CHECK_THROWS_AS(sub2->name("sub1"), CLI::OptionAlreadyAdded);
    sub2->ignore_underscore();
    CHECK_THROWS_AS(sub2->alias("les3"), CLI::OptionAlreadyAdded);
}
// test adding a subcommand via the pointer
TEST_CASE_METHOD(TApp, "ExistingSubcommandMatch", "[subcom]") {
    auto sshared = std::make_shared<CLI::App>("documenting the subcommand", "sub1");
    sshared->alias("sub2")->alias("sub3");

    CHECK("sub1" == sshared->get_name());
    app.add_subcommand("sub1");

    try {
        app.add_subcommand(sshared);
        // this should throw the next line should never be reached
        CHECK(!true);
    } catch(const CLI::OptionAlreadyAdded &oaa) {
        CHECK_THAT(oaa.what(), Contains("sub1"));
    }
    sshared->name("osub");
    app.add_subcommand("sub2");
    // now check that the aliases don't overlap
    try {
        app.add_subcommand(sshared);
        // this should throw the next line should never be reached
        CHECK(!true);
    } catch(const CLI::OptionAlreadyAdded &oaa) {
        CHECK_THAT(oaa.what(), Contains("sub2"));
    }
    // now check that disabled subcommands can be added regardless of name
    sshared->name("sub1");
    sshared->disabled();
    CHECK_NOTHROW(app.add_subcommand(sshared));
}

TEST_CASE_METHOD(TApp, "AliasErrorsInOptionGroup", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    auto *g2 = app.add_option_group("g1");
    auto *sub2 = g2->add_subcommand("sub2");

    // cannot alias to an existing subcommand even if it is in an option group
    CHECK_THROWS_AS(sub2->alias("sub1"), CLI::OptionAlreadyAdded);
    CHECK_THROWS_AS(sub1->alias("sub2"), CLI::OptionAlreadyAdded);

    sub1->alias("les1")->alias("les2")->alias("les3");
    sub2->alias("s2les1")->alias("s2les2")->alias("s2les3");

    CHECK_THROWS_AS(sub2->alias("les2"), CLI::OptionAlreadyAdded);
    CHECK_THROWS_AS(sub1->alias("s2les2"), CLI::OptionAlreadyAdded);

    CHECK_THROWS_AS(sub2->name("sub1"), CLI::OptionAlreadyAdded);
}

TEST_CASE("SharedSubTests: SharedSubcommand", "[subcom]") {
    double val{0.0}, val2{0.0}, val3{0.0}, val4{0.0};
    CLI::App app1{"test program1"};

    app1.add_option("-t", val2);
    auto *sub = app1.add_subcommand("", "empty name");
    sub->add_option("-v,--value", val);
    sub->add_option("-g", val4);
    CLI::App app2{"test program2"};
    app2.add_option("-m", val3);
    // extract an owning ptr from app1 and add it to app2
    auto subown = app1.get_subcommand_ptr(sub);
    // add the extracted subcommand to a different app
    app2.add_subcommand(std::move(subown));
    CHECK_THROWS_AS(app2.add_subcommand(CLI::App_p{}), CLI::IncorrectConstruction);
    input_t args1 = {"-m", "4.56", "-t", "5.93", "-v", "-3"};
    input_t args2 = {"-m", "4.56", "-g", "8.235"};
    std::reverse(std::begin(args1), std::end(args1));
    std::reverse(std::begin(args2), std::end(args2));
    app1.allow_extras();
    app1.parse(args1);

    app2.parse(args2);

    CHECK(-3.0 == val);
    CHECK(5.93 == val2);
    CHECK(4.56 == val3);
    CHECK(8.235 == val4);
}

TEST_CASE("SharedSubTests: SharedSubIndependent", "[subcom]") {
    double val{0.0}, val2{0.0}, val4{0.0};
    CLI::App_p app1 = std::make_shared<CLI::App>("test program1");
    app1->allow_extras();
    app1->add_option("-t", val2);
    auto *sub = app1->add_subcommand("", "empty name");
    sub->add_option("-v,--value", val);
    sub->add_option("-g", val4);

    // extract an owning ptr from app1 and add it to app2
    auto subown = app1->get_subcommand_ptr(sub);

    input_t args1 = {"-m", "4.56", "-t", "5.93", "-v", "-3"};
    input_t args2 = {"-m", "4.56", "-g", "8.235"};
    std::reverse(std::begin(args1), std::end(args1));
    std::reverse(std::begin(args2), std::end(args2));

    app1->parse(args1);
    // destroy the first parser
    app1 = nullptr;
    // parse with the extracted subcommand
    subown->parse(args2);

    CHECK(-3.0 == val);
    CHECK(5.93 == val2);
    CHECK(8.235 == val4);
}

TEST_CASE("SharedSubTests: SharedSubIndependentReuse", "[subcom]") {
    double val{0.0}, val2{0.0}, val4{0.0};
    CLI::App_p app1 = std::make_shared<CLI::App>("test program1");
    app1->allow_extras();
    app1->add_option("-t", val2);
    auto *sub = app1->add_subcommand("", "empty name");
    sub->add_option("-v,--value", val);
    sub->add_option("-g", val4);

    // extract an owning ptr from app1 and add it to app2
    auto subown = app1->get_subcommand_ptr(sub);

    input_t args1 = {"-m", "4.56", "-t", "5.93", "-v", "-3"};
    std::reverse(std::begin(args1), std::end(args1));
    auto args2 = args1;
    app1->parse(args1);

    // parse with the extracted subcommand
    subown->parse("program1 -m 4.56 -g 8.235", true);

    CHECK(-3.0 == val);
    CHECK(5.93 == val2);
    CHECK(8.235 == val4);
    val = 0.0;
    val2 = 0.0;
    CHECK("program1" == subown->get_name());
    // this tests the name reset in subcommand since it was automatic
    app1->parse(args2);
    CHECK(-3.0 == val);
    CHECK(5.93 == val2);
}

TEST_CASE_METHOD(ManySubcommands, "getSubtests", "[subcom]") {
    CLI::App_p sub2p = app.get_subcommand_ptr(sub2);
    CHECK(sub2 == sub2p.get());
    CHECK_THROWS_AS(app.get_subcommand_ptr(nullptr), CLI::OptionNotFound);
    CHECK_THROWS_AS(app.get_subcommand(nullptr), CLI::OptionNotFound);
    CLI::App_p sub3p = app.get_subcommand_ptr(2);
    CHECK(sub3 == sub3p.get());
}

TEST_CASE_METHOD(ManySubcommands, "defaultDisabledSubcommand", "[subcom]") {

    sub1->fallthrough();
    sub2->disabled_by_default();
    run();
    auto rem = app.remaining();
    CHECK(1u == rem.size());
    CHECK("sub2" == rem[0]);
    CHECK(sub2->get_disabled_by_default());
    sub2->disabled(false);
    CHECK(!sub2->get_disabled());
    run();
    // this should disable it again even though it was disabled
    rem = app.remaining();
    CHECK(1u == rem.size());
    CHECK("sub2" == rem[0]);
    CHECK(sub2->get_disabled_by_default());
    CHECK(sub2->get_disabled());
}

TEST_CASE_METHOD(ManySubcommands, "defaultEnabledSubcommand", "[subcom]") {

    sub2->enabled_by_default();
    run();
    auto rem = app.remaining();
    CHECK(rem.empty());
    CHECK(sub2->get_enabled_by_default());
    sub2->disabled();
    CHECK(sub2->get_disabled());
    run();
    // this should disable it again even though it was disabled
    rem = app.remaining();
    CHECK(rem.empty());
    CHECK(sub2->get_enabled_by_default());
    CHECK(!sub2->get_disabled());
}

// #572
TEST_CASE_METHOD(TApp, "MultiFinalCallbackCounts", "[subcom]") {

    int app_compl = 0;
    int sub_compl = 0;
    int subsub_compl = 0;
    int app_final = 0;
    int sub_final = 0;
    int subsub_final = 0;

    app.parse_complete_callback([&app_compl]() { app_compl++; });
    app.final_callback([&app_final]() { app_final++; });

    auto *sub = app.add_subcommand("sub");

    sub->parse_complete_callback([&sub_compl]() { sub_compl++; });
    sub->final_callback([&sub_final]() { sub_final++; });

    auto *subsub = sub->add_subcommand("subsub");

    subsub->parse_complete_callback([&subsub_compl]() { subsub_compl++; });
    subsub->final_callback([&subsub_final]() { subsub_final++; });

    SECTION("No specified subcommands") {
        args = {};
        run();

        CHECK(app_compl == 1);
        CHECK(app_final == 1);
        CHECK(sub_compl == 0);
        CHECK(sub_final == 0);
        CHECK(subsub_compl == 0);
        CHECK(subsub_final == 0);
    }

    SECTION("One layer of subcommands") {
        args = {"sub"};
        run();

        CHECK(app_compl == 1);
        CHECK(app_final == 1);
        CHECK(sub_compl == 1);
        CHECK(sub_final == 1);
        CHECK(subsub_compl == 0);
        CHECK(subsub_final == 0);
    }

    SECTION("Fully specified subcommands") {
        args = {"sub", "subsub"};
        run();

        CHECK(app_compl == 1);
        CHECK(app_final == 1);
        CHECK(sub_compl == 1);
        CHECK(sub_final == 1);
        CHECK(subsub_compl == 1);
        CHECK(subsub_final == 1);
    }
}

// From gitter issue
TEST_CASE_METHOD(TApp, "SubcommandInOptionGroupCallbackCount", "[subcom]") {

    int subcount{0};
    auto *group1 = app.add_option_group("FirstGroup");

    group1->add_subcommand("g1c1")->callback([&subcount]() { ++subcount; });

    args = {"g1c1"};
    run();
    CHECK(subcount == 1);
}

TEST_CASE_METHOD(TApp, "DotNotationSubcommand", "[subcom]") {
    std::string v1, v2, vbase;

    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = app.add_subcommand("sub2");
    sub1->add_option("--value", v1);
    sub2->add_option("--value", v2);
    app.add_option("--value", vbase);
    args = {"--sub1.value", "val1"};
    run();
    CHECK(v1 == "val1");

    args = {"--sub2.value", "val2", "--value", "base"};
    run();
    CHECK(v2 == "val2");
    CHECK(vbase == "base");
    v1.clear();
    v2.clear();
    vbase.clear();

    args = {"--sub2.value=val2", "--value=base"};
    run();
    CHECK(v2 == "val2");
    CHECK(vbase == "base");

    auto subs = app.get_subcommands();
    REQUIRE(!subs.empty());
    CHECK(subs.front()->get_name() == "sub2");
}

TEST_CASE_METHOD(TApp, "DotNotationSubcommandSingleChar", "[subcom]") {
    std::string v1, v2, vbase;

    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = app.add_subcommand("sub2");
    sub1->add_option("-v", v1);
    sub2->add_option("-v", v2);
    app.add_option("-v", vbase);
    args = {"--sub1.v", "val1"};
    run();
    CHECK(v1 == "val1");

    args = {"--sub2.v", "val2", "-v", "base"};
    run();
    CHECK(v2 == "val2");
    CHECK(vbase == "base");
    v1.clear();
    v2.clear();
    vbase.clear();

    args = {"--sub2.v=val2", "-vbase"};
    run();
    CHECK(v2 == "val2");
    CHECK(vbase == "base");

    auto subs = app.get_subcommands();
    REQUIRE(!subs.empty());
    CHECK(subs.front()->get_name() == "sub2");
}

TEST_CASE_METHOD(TApp, "DotNotationSubcommandRecursive", "[subcom]") {
    std::string v1, v2, v3, vbase;

    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = sub1->add_subcommand("sub2");
    auto *sub3 = sub2->add_subcommand("sub3");

    sub1->add_option("--value", v1);
    sub2->add_option("--value", v2);
    sub3->add_option("--value", v3);
    app.add_option("--value", vbase);
    args = {"--sub1.sub2.sub3.value", "val1"};
    run();
    CHECK(v3 == "val1");

    args = {"--sub1.sub2.value", "val2"};
    run();
    CHECK(v2 == "val2");

    args = {"--sub1.sub2.bob", "val2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);
    app.allow_extras();
    CHECK_NOTHROW(run());
    auto extras = app.remaining();
    CHECK(extras.size() == 2);
    CHECK(extras.front() == "--sub1.sub2.bob");
}

TEST_CASE_METHOD(TApp, "DotNotationSubcommandRecursive2", "[subcom]") {
    std::string v1, v2, v3, vbase;

    auto *sub1 = app.add_subcommand("sub1");
    auto *sub2 = sub1->add_subcommand("sub2");
    auto *sub3 = sub2->add_subcommand("sub3");

    sub1->add_option("--value", v1);
    sub2->add_option("--value", v2);
    sub3->add_option("--value", v3);
    app.add_option("--value", vbase);
    args = {"sub1.sub2.sub3", "--value", "val1"};
    run();
    CHECK(v3 == "val1");

    args = {"sub1.sub2", "--value", "val2"};
    run();
    CHECK(v2 == "val2");

    args = {"sub1.bob", "--value", "val2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"sub1.sub2.bob", "--value", "val2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    args = {"sub1.sub2.sub3.bob", "--value", "val2"};
    CHECK_THROWS_AS(run(), CLI::ExtrasError);

    app.allow_extras();
    CHECK_NOTHROW(run());
    auto extras = app.remaining();
    CHECK(extras.size() == 1);
    CHECK(extras.front() == "sub1.sub2.sub3.bob");
}

// Reported bug #903 on github
TEST_CASE_METHOD(TApp, "subcommandEnvironmentName", "[subcom]") {
    auto *sub1 = app.add_subcommand("sub1");
    std::string someFile;
    int sub1value{0};
    sub1->add_option("-f,--file", someFile)->envname("SOME_FILE")->required()->check(CLI::ExistingFile);
    sub1->add_option("-v", sub1value);
    auto *sub2 = app.add_subcommand("sub2");
    int completelyUnrelatedToSub1 = 0;
    sub2->add_option("-v,--value", completelyUnrelatedToSub1)->required();

    args = {"sub2", "-v", "111"};
    CHECK_NOTHROW(run());

    put_env("SOME_FILE", "notafile.txt");

    CHECK_NOTHROW(run());

    args = {"sub1", "-v", "111"};
    CHECK_THROWS_AS(run(), CLI::RequiredError);
    unset_env("SOME_FILE");
}
