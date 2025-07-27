// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "fuzzApp.hpp"
#include <algorithm>

namespace CLI {
/*
int32_t val32{0};
    int16_t val16{0};
    int8_t val8{0};
    int64_t val64{0};

    uint32_t uval32{0};
    uint16_t uval16{0};
    uint8_t uval8{0};
    uint64_t uval64{0};

    std::atomic<int64_t> atomicval64{0};
    std::atomic<uint64_t> atomicuval64{0};

    double v1{0};
    float v2{0};

    std::vector<double> vv1;
    std::vector<std::string> vstr;
    std::vector<std::vector<double>> vecvecd;
    std::vector<std::vector<std::string>> vvs;
    std::optional<double> od1;
    std::optional<std::string> ods;
    std::pair<double, std::string> p1;
    std::pair<std::vector<double>, std::string> p2;
    std::tuple<int64_t, uint16_t, std::optional<double>> t1;
    std::tuple<std::tuple<std::tuple<std::string, double, std::vector<int>>,std::string, double>,std::vector<int>,
std::optional<std::string>> tcomplex; std::string_view vstrv;

    bool flag1{false};
    int flagCnt{0};
    std::atomic<bool> flagAtomic{false};
    */
std::shared_ptr<CLI::App> FuzzApp::generateApp() {
    auto fApp = std::make_shared<CLI::App>("fuzzing App", "fuzzer");
    fApp->set_config("--config");
    fApp->set_help_all_flag("--help-all");
    fApp->add_flag("-a,--flag");
    fApp->add_flag("-b,--flag2,!--nflag2", flag1);
    fApp->add_flag("-c{34},--flag3{1}", flagCnt)->disable_flag_override();
    fApp->add_flag("-e,--flagA", flagAtomic);
    fApp->add_flag("--atd", doubleAtomic);

    fApp->add_option("-d,--opt1", val8);
    fApp->add_option("--opt2", val16);
    fApp->add_option("--opt3", val32);
    fApp->add_option("--opt4", val64);

    fApp->add_option("--opt5", uval8);
    fApp->add_option("--opt6", uval16);
    fApp->add_option("--opt7", uval32);
    fApp->add_option("--opt8", uval64);

    fApp->add_option("--aopt1", atomicval64);
    fApp->add_option("--aopt2", atomicuval64);

    fApp->add_option("--dopt1", v1);
    fApp->add_option("--dopt2", v2);

    auto *vgroup = fApp->add_option_group("vectors");

    vgroup->add_option("--vopt1", vv1);
    vgroup->add_option("--vopt2", vvs)->inject_separator();
    vgroup->add_option("--vopt3", vstr);
    vgroup->add_option("--vopt4", vecvecd)->inject_separator();

    fApp->add_option("--oopt1", od1);
    fApp->add_option("--oopt2", ods);

    fApp->add_option("--tup1", p1);
    fApp->add_option("--tup2", t1);
    fApp->add_option("--tup4", tcomplex);
    vgroup->add_option("--vtup", vectup);

    fApp->add_option("--dwrap", dwrap);
    fApp->add_option("--iwrap", iwrap);
    fApp->add_option("--swrap", swrap);
    // file checks
    fApp->add_option("--dexists")->check(ExistingDirectory);
    fApp->add_option("--fexists")->check(ExistingFile);
    fApp->add_option("--fnexists")->check(NonexistentPath);

    auto *sub = fApp->add_subcommand("sub1");

    sub->add_option("--sopt2", val16)->check(Range(1, 10));
    sub->add_option("--sopt3", val32)->check(PositiveNumber);
    sub->add_option("--sopt4", val64)->check(NonNegativeNumber);

    sub->add_option("--sopt5", uval8)->transform(Bound(6, 20));
    sub->add_option("--sopt6", uval16);
    sub->add_option("--sopt7", uval32);
    sub->add_option("--sopt8", uval64);

    sub->add_option("--saopt1", atomicval64);
    sub->add_option("--saopt2", atomicuval64);

    sub->add_option("--sdopt1", v1);
    sub->add_option("--sdopt2", v2);

    sub->add_option("--svopt1", vv1);
    sub->add_option("--svopt2", vvs);
    sub->add_option("--svopt3", vstr);
    sub->add_option("--svopt4", vecvecd);

    sub->add_option("--soopt1", od1);
    sub->add_option("--soopt2", ods);

    sub->add_option("--stup1", p1);
    sub->add_option("--stup2", t1);
    sub->add_option("--stup4", tcomplex);
    sub->add_option("--svtup", vectup);

    sub->add_option("--sdwrap", dwrap);
    sub->add_option("--siwrap", iwrap);

    auto *resgroup = fApp->add_option_group("outputOrder");

    resgroup->add_option("--vA", vstrA)->expected(0, 2)->multi_option_policy(CLI::MultiOptionPolicy::TakeAll);
    resgroup->add_option("--vB", vstrB)->expected(0, 2)->multi_option_policy(CLI::MultiOptionPolicy::TakeLast);
    resgroup->add_option("--vC", vstrC)->expected(0, 2)->multi_option_policy(CLI::MultiOptionPolicy::TakeFirst);
    resgroup->add_option("--vD", vstrD)->expected(0, 2)->multi_option_policy(CLI::MultiOptionPolicy::Reverse);
    resgroup->add_option("--vS", val32)->expected(0, 2)->multi_option_policy(CLI::MultiOptionPolicy::Sum);
    resgroup->add_option("--vM", mergeBuffer)->expected(0, 2)->multi_option_policy(CLI::MultiOptionPolicy::Join);
    resgroup->add_option("--vE", vstrE)->expected(2, 4)->delimiter(',');

    auto *vldtr = fApp->add_option_group("validators");

    validator_strings.resize(10);
    vldtr->add_option("--vdtr1", validator_strings[0])->join()->check(CLI::PositiveNumber);
    vldtr->add_option("--vdtr2", validator_strings[1])->join()->check(CLI::NonNegativeNumber);
    vldtr->add_option("--vdtr3", validator_strings[2])->join()->check(CLI::NonexistentPath);
    vldtr->add_option("--vdtr4", validator_strings[3])->join()->check(CLI::Range(7, 3456));
    vldtr->add_option("--vdtr5", validator_strings[4])
        ->join()
        ->check(CLI::Range(std::string("aa"), std::string("zz"), "string range"));
    vldtr->add_option("--vdtr6", validator_strings[5])->join()->check(CLI::TypeValidator<double>());
    vldtr->add_option("--vdtr7", validator_strings[6])->join()->check(CLI::TypeValidator<bool>());
    vldtr->add_option("--vdtr8", validator_strings[7])->join()->check(CLI::ValidIPV4);
    vldtr->add_option("--vdtr9", validator_strings[8])->join()->transform(CLI::Bound(2, 255));
    return fApp;
}

bool FuzzApp::compare(const FuzzApp &other) const {
    if(val32 != other.val32) {
        return false;
    }
    if(val16 != other.val16) {
        return false;
    }
    if(val8 != other.val8) {
        return false;
    }
    if(val64 != other.val64) {
        return false;
    }

    if(uval32 != other.uval32) {
        return false;
    }
    if(uval16 != other.uval16) {
        return false;
    }
    if(uval8 != other.uval8) {
        return false;
    }
    if(uval64 != other.uval64) {
        return false;
    }

    if(atomicval64 != other.atomicval64) {
        return false;
    }
    if(atomicuval64 != other.atomicuval64) {
        return false;
    }

    if(v1 != other.v1) {
        return false;
    }
    if(v2 != other.v2) {
        return false;
    }

    if(vv1 != other.vv1) {
        return false;
    }
    if(vstr != other.vstr) {
        return false;
    }

    if(vecvecd != other.vecvecd) {
        return false;
    }
    if(vvs != other.vvs) {
        return false;
    }
    if(od1 != other.od1) {
        return false;
    }
    if(ods != other.ods) {
        return false;
    }
    if(p1 != other.p1) {
        return false;
    }
    if(p2 != other.p2) {
        return false;
    }
    if(t1 != other.t1) {
        return false;
    }
    if(tcomplex != other.tcomplex) {
        return false;
    }
    if(tcomplex2 != other.tcomplex2) {
        return false;
    }
    if(vectup != other.vectup) {
        return false;
    }
    if(vstrv != other.vstrv) {
        return false;
    }

    if(flag1 != other.flag1) {
        return false;
    }
    if(flagCnt != other.flagCnt) {
        return false;
    }
    if(flagAtomic != other.flagAtomic) {
        return false;
    }

    if(iwrap.value() != other.iwrap.value()) {
        return false;
    }
    if(dwrap.value() != other.dwrap.value()) {
        return false;
    }
    if(swrap.value() != other.swrap.value()) {
        return false;
    }
    if(buffer != other.buffer) {
        return false;
    }
    if(intbuffer != other.intbuffer) {
        return false;
    }
    if(doubleAtomic != other.doubleAtomic) {
        return false;
    }

    // for testing restrictions and reduction methods
    if(vstrA != other.vstrA) {
        return false;
    }
    if(vstrB != other.vstrB) {
        return false;
    }
    if(vstrC != other.vstrC) {
        return false;
    }
    if(vstrD != other.vstrD) {
        // the return result if reversed so it can alternate
        std::vector<std::string> res = vstrD;
        std::reverse(res.begin(), res.end());
        if(res != other.vstrD) {
            return false;
        }
    }
    if(vstrE != other.vstrE) {
        return false;
    }
    if(vstrF != other.vstrF) {
        return false;
    }
    if(mergeBuffer != other.mergeBuffer) {
        return false;
    }
    if(validator_strings != other.validator_strings) {
        return false;
    }
    // now test custom string_options
    if(custom_string_options.size() != other.custom_string_options.size()) {
        return false;
    }
    for(std::size_t ii = 0; ii < custom_string_options.size(); ++ii) {
        if(*custom_string_options[ii] != *other.custom_string_options[ii]) {
            return false;
        }
    }
    // now test custom vector_options
    if(custom_vector_options.size() != other.custom_vector_options.size()) {
        return false;
    }
    for(std::size_t ii = 0; ii < custom_vector_options.size(); ++ii) {
        if(*custom_vector_options[ii] != *other.custom_vector_options[ii]) {
            return false;
        }
    }
    return true;
}

//<option>name_string</option>
//<vector>name_string</vector>
//<flag>name_string</flag>
/** generate additional options based on a string config*/
std::size_t FuzzApp::add_custom_options(CLI::App *app, std::string &description_string) {
    std::size_t current_index{0};
    while(description_string.size() - 5 > current_index && description_string[current_index] == '<') {
        if(description_string.compare(current_index, 7, "<option") == 0) {
            auto end_option = description_string.find("</option>", current_index + 8);
            if(end_option == std::string::npos) {
                break;
            }
            auto header_close = description_string.find_last_of('>', end_option);
            if(header_close == std::string::npos || header_close < current_index) {
                break;
            }
            std::string name = description_string.substr(header_close + 1, end_option - header_close - 1);
            custom_string_options.push_back(std::make_shared<std::string>());
            app->add_option(name, *(custom_string_options.back()));
            current_index = end_option + 9;
        } else if(description_string.compare(current_index, 5, "<flag") == 0) {
            auto end_option = description_string.find("</flag>", current_index + 6);
            if(end_option == std::string::npos) {
                break;
            }
            auto header_close = description_string.find_last_of('>', end_option);
            if(header_close == std::string::npos || header_close < current_index) {
                break;
            }
            std::string name = description_string.substr(header_close + 1, end_option - header_close - 1);
            custom_string_options.push_back(std::make_shared<std::string>());
            app->add_option(name, *(custom_string_options.back()));
            current_index = end_option + 7;
        } else if(description_string.compare(current_index, 7, "<vector") == 0) {
            auto end_option = description_string.find("</vector>", current_index + 8);
            if(end_option == std::string::npos) {
                break;
            }
            auto header_close = description_string.find_last_of('>', end_option);
            if(header_close == std::string::npos || header_close < current_index) {
                break;
            }
            std::string name = description_string.substr(header_close + 1, end_option - header_close - 1);
            custom_vector_options.push_back(std::make_shared<std::vector<std::string>>());
            app->add_option(name, *(custom_string_options.back()));
            current_index = end_option + 9;
        } else {
            if(isspace(description_string[current_index]) != 0) {
                ++current_index;
            } else {
                break;
            }
        }
    }
    return current_index;
}

}  // namespace CLI
