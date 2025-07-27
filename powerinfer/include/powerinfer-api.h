#pragma once

#    if defined(_WIN32) && !defined(__MINGW32__)
#        ifdef POWERINFER_BUILD
#            define POWERINFER_API __declspec(dllexport)
#        else
#            define POWERINFER_API __declspec(dllimport)
#        endif
#    else
#        define POWERINFER_API __attribute__ ((visibility ("default")))
#    endif
