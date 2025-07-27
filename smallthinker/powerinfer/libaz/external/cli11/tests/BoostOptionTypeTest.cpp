// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_helper.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/slist.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/vector.hpp>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

TEMPLATE_TEST_CASE("Boost container single",
                   "[boost][optional]",
                   (boost::container::small_vector<int, 2>),
                   (boost::container::small_vector<int, 3>),
                   boost::container::flat_set<int>,
                   boost::container::stable_vector<int>,
                   boost::container::slist<int>) {
    TApp tapp;
    TestType cv;
    CLI::Option *opt = tapp.app.add_option("-v", cv);

    tapp.args = {"-v", "1", "-1", "-v", "3", "-v", "-976"};
    tapp.run();
    CHECK(tapp.app.count("-v") == 4u);
    CHECK(cv.size() == 4u);
    opt->check(CLI::PositiveNumber.application_index(0));
    opt->check((!CLI::PositiveNumber).application_index(1));
    CHECK_NOTHROW(tapp.run());
    CHECK(cv.size() == 4u);
    // v[3] would be negative
    opt->check(CLI::PositiveNumber.application_index(3));
    CHECK_THROWS_AS(tapp.run(), CLI::ValidationError);
}

using isp = std::pair<int, std::string>;

TEMPLATE_TEST_CASE("Boost container pair",
                   "[boost][optional]",
                   boost::container::stable_vector<isp>,
                   (boost::container::small_vector<isp, 2>),
                   boost::container::flat_set<isp>,
                   boost::container::slist<isp>,
                   boost::container::vector<isp>,
                   (boost::container::flat_map<int, std::string>)) {

    TApp tapp;
    TestType cv;

    tapp.app.add_option("--dict", cv);

    tapp.args = {"--dict", "1", "str1", "--dict", "3", "str3"};

    tapp.run();
    CHECK(2u == cv.size());

    tapp.args = {"--dict", "1", "str1", "--dict", "3", "--dict", "-1", "str4"};
    tapp.run();
    CHECK(3u == cv.size());
}

using tup_obj = std::tuple<int, std::string, double>;

TEMPLATE_TEST_CASE("Boost container tuple",
                   "[boost][optional]",
                   (boost::container::small_vector<tup_obj, 3>),
                   boost::container::stable_vector<tup_obj>,
                   boost::container::flat_set<tup_obj>,
                   boost::container::slist<tup_obj>) {
    TApp tapp;
    TestType cv;

    tapp.app.add_option("--dict", cv);

    tapp.args = {"--dict", "1", "str1", "4.3", "--dict", "3", "str3", "2.7"};

    tapp.run();
    CHECK(2u == cv.size());

    tapp.args = {"--dict", "1", "str1", "4.3", "--dict", "3", "str3", "2.7", "--dict", "-1", "str4", "-1.87"};
    tapp.run();
    CHECK(3u == cv.size());
}

using icontainer1 = boost::container::vector<int>;
using icontainer2 = boost::container::flat_set<int>;
using icontainer3 = boost::container::slist<int>;

TEMPLATE_TEST_CASE("Boost container container",
                   "[boost][optional]",
                   std::vector<icontainer1>,
                   boost::container::slist<icontainer1>,
                   boost::container::flat_set<icontainer1>,
                   (boost::container::small_vector<icontainer1, 2>),
                   std::vector<icontainer2>,
                   boost::container::slist<icontainer2>,
                   boost::container::flat_set<icontainer2>,
                   boost::container::stable_vector<icontainer2>,
                   (boost::container::static_vector<icontainer2, 10>),
                   boost::container::slist<icontainer3>,
                   boost::container::flat_set<icontainer3>,
                   (boost::container::static_vector<icontainer3, 10>)) {

    TApp tapp;
    TestType cv;

    tapp.app.add_option("--dict", cv);

    tapp.args = {"--dict", "1", "2", "4", "--dict", "3", "1"};

    tapp.run();
    CHECK(2u == cv.size());

    tapp.args = {"--dict", "1", "2", "4", "--dict", "3", "1", "--dict", "3", "--dict",
                 "3",      "3", "3", "3", "3",      "3", "3", "3",      "3", "-3"};
    tapp.run();
    CHECK(4u == cv.size());
}
