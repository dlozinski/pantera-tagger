#ifndef POLISH_SEGM_DISAMB_H_
#define POLISH_SEGM_DISAMB_H_

#include <sstream>
#include <string>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/regex.hpp>

#include <nlpcommon/exception.h>
#include <nlpcommon/segm_disamb.h>
#include <nlpcommon/util.h>

namespace NLPCommon {

using std::wstring;

extern const wchar_t* default_polish_segm_disamb_config;

template<class Lexeme>
class PolishSegmDisambiguator : public SegmDisambiguator<Lexeme>
{
private:
    std::vector<std::pair<boost::wregex, string> > rules;

    bool shouldBeSeparate(std::vector<Lexeme>& text, int i) {
        wstring orth = text[i].getOrth();
        boost::to_lower(orth, get_locale("pl_PL"));

        // TODO: process according to configuration

        return true;
    }


public:
    PolishSegmDisambiguator() {
        std::wstring config(default_polish_segm_disamb_config);
        std::wistringstream config_stream(config);
        this->loadConfiguration(config_stream);
    }

    void loadConfiguration(std::wistream& stream) {
        rules.clear();
        wstring line;
        while (!stream.eof()) {
            std::getline(stream, line);
            boost::trim(line);
            if (line[0] == L'#' || line.empty())
                continue;
            std::vector<wstring> parts;
            boost::split(parts, line, boost::is_space(get_locale("pl_PL")),
                    boost::token_compress_on);

            if (parts.size() < 2) {
                throw Exception(boost::str(boost::format(
                                "Not enough tokens in segmentation "
                                "disambiguator configuration line: %1%")
                            % wstring_to_utf8(line)));
            }

            rules.push_back(std::make_pair(
                        boost::wregex(parts[0]),
                        wstring_to_utf8(parts[1])));
        }
    }

    void disambiguateSegmentation(std::vector<Lexeme>& text) {
        int size = text.size();
        bool in_ambiguous = false;
        int num_fragments;
        int first_i, second_i;
        for (int i = 0; i < size; i++) { 
            const Lexeme& lex = text[i];
            switch (lex.getType()) {
                case Lexeme::START_OF_AMBIGUITY:
                    if (in_ambiguous) {
                        throw Exception("PolishSegmDisambiguator cannot "
                                "handle nested ambiguities.");
                    }
                    in_ambiguous = true;
                    num_fragments = 0;
                    break;

                case Lexeme::END_OF_AMBIGUITY:
                    assert(in_ambiguous);
                    if (num_fragments == 1) {
                        text[first_i].setType(Lexeme::ACCEPTED_FRAGMENT);
                    } else if (num_fragments == 2) {
                        if (second_i - first_i != 4 || second_i != i - 2) {
                            if (second_i - first_i == 2 && second_i == i - 4) {
                                std::swap(first_i, second_i);
                            } else {
                                throw Exception("PolishSegmDisambiguator got "
                                        "unexpected ambiguity. Only [BEG][UNR]"
                                        "[SEG][NS][SEG][UNR][SEG][END] is "
                                        "supported.");
                            }
                        }
                        bool separate = shouldBeSeparate(text, second_i + 1);
                        text[first_i].setType(
                                separate ? Lexeme::ACCEPTED_FRAGMENT :
                                    Lexeme::REJECTED_FRAGMENT);
                        text[second_i].setType(
                                separate ? Lexeme::REJECTED_FRAGMENT :
                                    Lexeme::ACCEPTED_FRAGMENT);
                    }
                    in_ambiguous = false;
                    break;

                case Lexeme::UNRESOLVED_FRAGMENT:
                    num_fragments++;
                    if (num_fragments == 1) {
                        first_i = i;
                    } else if (num_fragments == 2) {
                        second_i = i;
                    } else {
                        throw Exception("PolishSegmDisambiguator cannot "
                                "handle ambiguities with more than 2 "
                                "possibilities.");
                    }
                    break;

                default:
                    // do nothing
                    ;
            }
		}
    }
};

} // namespace NLPCommon

#endif /* POLISH_SEGM_DISAMB_H_ */