#include <nlpcommon/polish_segm_disamb.h>

namespace NLPCommon {

const wchar_t* default_polish_segm_disamb_config =
    L"piekłem separate\n"
    L"czekał.m separate\n"
    L"miałem separate\n"
    L"winnym together\n"
    L".*łem together\n"
    L".*ś together\n"
    ;

}  // namespace NLPCommon

