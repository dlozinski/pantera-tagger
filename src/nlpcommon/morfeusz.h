/*
 * writer.h
 *
 *  Created on: Jan 2, 2010
 *      Author: accek
 */

#ifndef MORFEUSZANALYZER_H_
#define MORFEUSZANALYZER_H_

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/range/iterator_range.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <morfeusz.h>
#include <guesser_api.h>

#include <nlpcommon/lexeme.h>
#include <nlpcommon/progress.h>
#include <nlpcommon/exception.h>
#include <nlpcommon/util.h>
#include <nlpcommon/tagset.h>
#include <nlpcommon/polish_tagset_convert.h>

#ifndef MORFEUSZ_TAGSET
#define MORFEUSZ_TAGSET "ipipan"
#endif

#ifndef ODGADYWACZ_TAGSET
#define ODGADYWACZ_TAGSET "ipipan"
#endif

namespace NLPCommon {

using std::vector;
using std::wstring;
using std::string;
using std::ifstream;

class MorfeuszAnalyzerException : public Exception
{
public:
    MorfeuszAnalyzerException(const string& msg) : Exception(msg) { }
    virtual ~MorfeuszAnalyzerException() throw () { }
};

template<class Lexeme = DefaultLexeme>
class MorfeuszAnalyzer
{
public:
    typedef typename Lexeme::tag_type tag_type;

private:
    typedef boost::split_iterator<string::const_iterator> string_split_iterator;

    const Tagset* out_tagset;
    const Tagset* morf_tagset;
    TagsetConverter<tag_type>* morf_converter;
    const Tagset* odg_tagset;
    TagsetConverter<tag_type>* odg_converter;

    bool use_odgadywacz;
    bool quiet;

    // lowercase form -> vector of (base, tag)
    std::map<wstring, std::vector<std::pair<wstring, string> > > morph_dict;

    void parseMorfeuszTagSuffix(
            tag_type& tag,
            const PartOfSpeech* pos,
            int category_offset,
            const string_split_iterator& part,
            const wstring& base,
            Lexeme& lex) {
        if (part.eof()) {
            int num_cats = pos->getCategories().size();
            while (category_offset < num_cats) {
                const Category* cat = pos->getCategories()[category_offset];
                int cindex = morf_tagset->getCategoryIndex(cat);
                tag.setValue(cindex, cat->getIndex("[none]"));
                category_offset++;
            }

            tag_type out_tag = morf_converter->convert(lex, tag);
            lex.addTagBase(out_tag, base);
            lex.addAllowedTag(out_tag);
            return;
        }

        const Category* cat = pos->getCategories()[category_offset];
        bool is_required_cat = pos->isRequiredCategory(category_offset);
        int cindex = morf_tagset->getCategoryIndex(cat);
        string_split_iterator new_part = part;
        ++new_part;

        if (part->front() == '_') {
            int num_values = cat->getValues().size();
            for (int i = is_required_cat ? 0 : 1; i < num_values; i++) {
                tag.setValue(cindex, i);
                parseMorfeuszTagSuffix(tag, pos, category_offset + 1,
                        new_part, base, lex);
            }
        } else {
            for (string_split_iterator value_it =
                    boost::make_split_iterator(*part, boost::token_finder(
                            boost::is_any_of(".")));
                    value_it != string_split_iterator(); ++value_it) {
                tag.setValue(cindex,
                        cat->getIndex(boost::copy_range<string>(*value_it)));
                parseMorfeuszTagSuffix(tag, pos, category_offset + 1,
                        new_part, base, lex);
            }
        }
    }

    void parseMorfeuszTag(const string_split_iterator& mtag, const wstring& base,
            Lexeme& lex) {
        string_split_iterator cat_it =
                boost::make_split_iterator(*mtag, boost::token_finder(
                            boost::is_any_of(":")));
        const PartOfSpeech* pos = morf_tagset->getPartOfSpeech(
                boost::copy_range<string>(*cat_it));
        int pos_idx = morf_tagset->getPartOfSpeechIndex(pos);
        tag_type tag;
        tag.clear();
        tag.setPos(pos_idx);
        parseMorfeuszTagSuffix(tag, pos, 0, ++cat_it, base, lex);
    }

    void parseMorfeuszTags(const string& mtags, const wstring& base,
            Lexeme& lex) {
        for (string_split_iterator tag_it =
                boost::make_split_iterator(mtags, boost::token_finder(
                        boost::is_any_of("|")));
                tag_it != string_split_iterator(); ++tag_it) {
            parseMorfeuszTag(tag_it, base, lex);
        }
    }

	void parseOdgadywaczResponse(const string& forms, Lexeme& lex) {
        bool got_form = false;
        for (string_split_iterator segment_it =
                boost::make_split_iterator(forms, boost::token_finder(
                        boost::is_any_of("\n")));
                segment_it != string_split_iterator(); ++segment_it) {
            if (segment_it->begin() == segment_it->end())
                break;

            string_split_iterator interp_it = boost::make_split_iterator(
                    *segment_it, boost::token_finder(boost::is_any_of("\t")));
            wstring odg_orth = utf8_to_wstring(
                    boost::copy_range<string>(*interp_it));
            if (got_form) {
                if (!quiet) {
                    std::cerr << "Odgadywacz generated unexpected segment '" <<
                        wstring_to_utf8(odg_orth) << "', ignoring." << std::endl;
                }
                continue;
            } else {
                got_form = true;
            }

            if (odg_orth != lex.getOrth() && !quiet) {
                std::cerr << "Odgadywacz generated form '" <<
                    wstring_to_utf8(odg_orth) << "', but expected '" <<
                    lex.getUtf8Orth() << "'" << std::endl;
            }
            
            for (++interp_it; interp_it != string_split_iterator(); ++interp_it)
            {
                vector<boost::iterator_range<string::const_iterator> > parts;
                boost::split(parts, *interp_it, boost::is_any_of(" "));
                tag_type tag = tag_type::parseString(odg_tagset,
                        boost::copy_range<string>(parts[1]));
                tag_type out_tag = odg_converter->convert(lex, tag);
                lex.addAllowedTag(out_tag);
                lex.addTagBase(out_tag,
                        utf8_to_wstring(boost::copy_range<string>(parts[0])));
            }
        }
    }

	void parseDictEntry(const std::vector<std::pair<wstring, string> >& entry,
            Lexeme& lex) {
        typedef std::pair<wstring, string> ss_type;
        BOOST_FOREACH(const ss_type& ss, entry) {
            tag_type tag = tag_type::parseString(out_tagset, ss.second);
            lex.addAllowedTag(tag);
            lex.addTagBase(tag, ss.first);
        }
    }

public:
    MorfeuszAnalyzer(const Tagset* out_tagset, bool use_odgadywacz = true)
        : out_tagset(out_tagset),
          morf_tagset(load_tagset(MORFEUSZ_TAGSET)),
          morf_converter(PolishTagsetConverter<tag_type>::getSharedInstance(
                      morf_tagset, out_tagset)),
          odg_tagset(load_tagset(ODGADYWACZ_TAGSET)),
          odg_converter(PolishTagsetConverter<tag_type>::getSharedInstance(
                      odg_tagset, out_tagset)),
          use_odgadywacz(use_odgadywacz),
          quiet(false)
    {
    }

    void setQuiet(bool value = true) {
        quiet = value;
    }

    // Format of morphological dictionary file:
    // 
    // |word
    // | [base_form]
    // | tag
    // | tag
    // | [base_form]
    // | tag
    // 
    // Full-line comments starting with # are allowed.
    //
    void loadMorphDict(const string& filename) {
        wstring key;
        wstring base;
        std::vector<std::pair<wstring, string> > interps;

        char line_buffer[1024];
        ifstream stream(filename.c_str());
        stream.exceptions(ifstream::badbit);
        while (!stream.eof()) {
            stream.getline(line_buffer, sizeof(line_buffer));
            string line(line_buffer);
            bool cont = !line.empty() && boost::is_space()(line[0]);
            boost::trim(line);

            if (line.empty() || line[0] == '#')
                continue;

            if (cont) {
                // Continuing existing entry.
                if (key.empty()) {
                    throw Exception(boost::str(boost::format(
                                    "loadMorphDict() expected definition "
                                    "at the beginning of the file. "
                                    "Definition starts with non-whitespace, "
                                    "but a non-comment line starting with "
                                    "whitespace found. (Line: '%1%')")
                                % line));
                }

                if (line[0] == '[') {
                    // Definition of base form.
                    if (line[line.length() - 1] != ']') {
                        throw Exception(boost::str(boost::format(
                                        "loadMorphDict() expected ']' at "
                                        "end of line starting with '['. "
                                        "(Line: '%1%')")
                                    % line));
                    }
                    base = utf8_to_wstring(line.substr(1, line.length() - 2));
                } else {
                    interps.push_back(std::make_pair(base, line));
                }
            } else {
                // New entry.
                if (!key.empty()) {
                    morph_dict[key] = interps;
                }

                base.clear();
                interps.clear();
                key = utf8_to_wstring(line);
                boost::to_lower(key, get_locale("pl_PL.UTF-8"));
            }
        }

        if (!key.empty()) {
            morph_dict[key] = interps;
        }
    }

	vector<Lexeme> analyzeText(const vector<Lexeme>& text) {
        vector<Lexeme> ret;
        int tidx = -1;
		BOOST_FOREACH(const Lexeme& lex, text) {
            tidx++;

            if (lex.getType() != Lexeme::SEGMENT) {
                ret.push_back(lex);
                continue;
            }

            morfeusz_set_option(MORFOPT_ENCODING, MORFEUSZ_UTF_8);
            InterpMorf* interps = morfeusz_analyse(
                    const_cast<char*>(lex.getUtf8Orth().c_str()));
            //std::cerr << "To morf: " << lex.getUtf8Orth().c_str() << std::endl;
            
            // TODO: Check first if there is ambiguity. If so, disable interpretations to
            //       skip.

            Lexeme current_lex;
            int segm = -1;
            for (int i = 0; ; i++) {
                InterpMorf& interp = interps[i];
                if (interp.p == -1)
                    break;
                if (interp.p + 1 != interp.k || interp.p < segm
                        || interp.p > segm + 1) {
                    if (!quiet) {
                        std::cerr << 
								boost::format("Ambiguous interpretation "
                                    "returned by Morfeusz for word '%1%' "
                                    "(edge %2% -> %3%, expected %4% -> %5%, "
                                    "forma='%6%')")
                                % lex.getUtf8Orth() % interp.p % interp.k
                                % i % (i + 1) % interp.forma << std::endl
                                << "     ";
                        for (int i = std::max<int>(0, tidx - 3);
                                i <= std::min<int>(text.size(), tidx + 3);
                                i++)
                            std::cerr << text[i].getUtf8Orth() << ' ';
                        std::cerr << std::endl << std::endl;
                    }
                    continue;
                }

                if (interp.p > segm) {
                    if (segm != -1) {
                        ret.push_back(current_lex);
                        //std::cerr << "Morf: " << current_lex.getUtf8Orth() << std::endl;
                        ret.push_back(Lexeme(Lexeme::NO_SPACE));
                    }
                    current_lex = Lexeme(Lexeme::SEGMENT);
                    current_lex.setUtf8Orth(interp.forma);
                    segm = interp.p;
                }

                //std::cerr << boost::format("morfeusz: %1% %2% %3% %4% %5%\n") %
                //    interp.p % interp.k % interp.forma % interp.haslo % interp.interp;

                if (!morph_dict.empty()) {
                    wstring morph_key(utf8_to_wstring(interp.forma));
                    boost::to_lower(morph_key, get_locale("pl_PL.UTF-8"));

                    std::map<wstring, std::vector<std::pair<wstring, string> > >
                       ::const_iterator i = morph_dict.find(morph_key);
                    if (i != morph_dict.end()) {
                        parseDictEntry(i->second, current_lex);
                        continue;
                    }
                }

                if (interp.interp == NULL) {
                    string forma_copy(interp.forma);
                    wstring forma = utf8_to_wstring(forma_copy);

                    SetCorpusEncoding(GUESSER_UTF8);

                    if (use_odgadywacz) {
                        try {
                            //std::cerr << "To odg: " << interp.forma << ' ' << lex.getUtf8Orth() << std::endl;
                            string forms = GuessForm(forma_copy.c_str());
                            //std::cerr << "Odg:" << forms << " LEX: " << lex.getUtf8Orth() << std::endl;
                            parseOdgadywaczResponse(forms, current_lex);
                        } catch (std::exception const& e) {
                            if (!quiet) {
                                std::cerr << 
                                        boost::format("Odgadywacz failed for "
                                                "word '%1%'. Error was: %2%.")
                                        % interp.forma % e.what() << std::endl;
                            }
                        }
                    }

                    // Add "ign".
                    tag_type tag = tag_type::parseString(out_tagset, string("ign"));
                    if (!current_lex.isAllowedTag(tag)) {
                        current_lex.addAllowedTag(tag);
                        current_lex.addTagBase(tag, forma);
                    }

                    morfeusz_set_option(MORFOPT_ENCODING, MORFEUSZ_UTF_8);
                    interps = morfeusz_analyse(
                            const_cast<char*>(lex.getUtf8Orth().c_str()));
                } else {
                    //std::cerr << "Morf:" << interp.forma << ' ' << interp.haslo << ' ' << interp.interp << std::endl;
                    parseMorfeuszTags(interp.interp,
                            utf8_to_wstring(interp.haslo == NULL ?
                                interp.forma : interp.haslo), current_lex);
                }
            }
            if (segm >= 0) {
                ret.push_back(current_lex);
                //std::cerr << "Morf: " << current_lex.getUtf8Orth() << std::endl;
            }
		}

        return ret;
	}
};


} // namespace NLPCommon

#endif /* MORFEUSZANALYZER_H_ */
