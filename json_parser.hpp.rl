#ifndef HLCUP_JSON_PARSER_HPP__
#define HLCUP_JSON_PARSER_HPP__

#include "common.hpp"

namespace hlcup {

%%{

    machine json_parser;

    json_char = [^"\\] @{ *string_val += fc; }
              | '\\' ( '\\' @{ *string_val += '\\'; }
                     | '/'  @{ *string_val += '/'; }
                     | 'b'  @{ *string_val += '\b'; }
                     | 'f'  @{ *string_val += '\f'; }
                     | 'n'  @{ *string_val += '\n'; }
                     | 'r'  @{ *string_val += '\r'; }
                     | 't'  @{ *string_val += '\t'; }
                     )
              | '\\u' %{ uni_char = 0; } ( [0-9] @{ uni_char = (uni_char << 4) | (fc - '0'); }
                                         | [a-f] @{ uni_char = (uni_char << 4) | (fc - 'a' + 10); }
                                         | [A-F] @{ uni_char = (uni_char << 4) | (fc - 'A' + 10); } ){4}
                                         %{ ucs2_to_utf8(uni_char, string_val); }
              ;

    action json_err {
        return false;
    }

    json_string = '"' @{ string_val->clear(); } json_char** '"';
    json_int  = [0-9] @{ *int_val = fc - '0'; } ([0-9] @{ *int_val = *int_val * 10 + (fc - '0'); } )**
              | '-' [0-9] @{ *int_val = fc - '0'; } ([0-9] @{ *int_val = *int_val * 10 + (fc - '0'); } )** %{ *int_val = -*int_val; }
              ;
}%%

void ucs2_to_utf8 (int32_t ucs2, std::string *utf8)
{
    if (ucs2 < 0x80) {
        *utf8 += static_cast<char>(ucs2);
    } else if (ucs2 < 0x800) {
        *utf8 += static_cast<char>((ucs2 >> 6)   | 0xC0);
        *utf8 += static_cast<char>((ucs2 & 0x3F) | 0x80);
    } else if (ucs2 < 0xFFFF) {
        *utf8 += static_cast<char>( ((ucs2 >> 12)       ) | 0xE0 );
        *utf8 += static_cast<char>( ((ucs2 >> 6 ) & 0x3F) | 0x80 );
        *utf8 += static_cast<char>( ((ucs2      ) & 0x3F) | 0x80 );
    } else {
        *utf8 += static_cast<char>( 0xF0 | (ucs2 >> 18) );
        *utf8 += static_cast<char>( 0x80 | ((ucs2 >> 12) & 0x3F) );
        *utf8 += static_cast<char>( 0x80 | ((ucs2 >> 6) & 0x3F) );
        *utf8 += static_cast<char>( 0x80 | ((ucs2 & 0x3F)) );
    }
}

struct JsonParser {
    int cs;
    std::string *string_val;
    int *int_val;
    int32_t uni_char;
};

%%{
    machine user_parser;
    include json_parser;

    int_key = "id" %{ e->mask.set(Param::kId); int_val = &e->id; }
            | "birth_date" %{ e->mask.set(Param::kBirthDate); int_val = &e->birth_date; }
            ;
    string_key = "email" %{ e->mask.set(Param::kEmail);          string_val = &e->email; }
               | "first_name" %{ e->mask.set(Param::kFirstName); string_val = &e->first_name; }
               | "last_name" %{ e->mask.set(Param::kLastName);   string_val = &e->last_name; }
               ;
    json_data = '"' int_key '"' space** ':' space** json_int
              | '"' string_key '"' space** ':' space** json_string
              | '"gender"' space** ':' space** '"m"' %{ e->mask.set(Param::kGender); e->gender = Gender::kMale; }
              | '"gender"' space** ':' space** '"f"' %{ e->mask.set(Param::kGender); e->gender = Gender::kFemale; }
              ;
    main := [^\{]** '{' space** json_data (',' space** json_data space** )** '}' @{ return p + 1; };

    write data;
}%%

struct UserParser : JsonParser {
    void reset() {
        %% write init;
    }

    const char *parse(const char *p, const char *pe, User *e) {
        using Param = User::Param;
        %% write exec;
        return nullptr;
    }
};

%%{
    machine visit_parser;
    include json_parser;

    int_key = "id" %{ e->mask.set(Param::kId); int_val = &e->id; }
            | "location" %{ e->mask.set(Param::kLocationId); int_val = &e->location_id; }
            | "user" %{ e->mask.set(Param::kUserId); int_val = &e->user_id; }
            | "visited_at" %{ e->mask.set(Param::kVisitedAt); int_val = &e->visited_at; }
            ;
    json_data = '"' int_key '"' space** ':' space** json_int
              | '"mark"' space** ':' space** ( '0' %{ e->mask.set(Param::kMark); e->mark = 0; }
                                             | '1' %{ e->mask.set(Param::kMark); e->mark = 1; }
                                             | '2' %{ e->mask.set(Param::kMark); e->mark = 2; }
                                             | '3' %{ e->mask.set(Param::kMark); e->mark = 3; }
                                             | '4' %{ e->mask.set(Param::kMark); e->mark = 4; }
                                             | '5' %{ e->mask.set(Param::kMark); e->mark = 5; }
                                             )
              ;
    main := [^\{]** '{' space** json_data (',' space** json_data space** )** '}' @{ return p + 1; };

    write data;
}%%

struct VisitParser : JsonParser {
    void reset() {
        %% write init;
    }

    const char *parse(const char *p, const char *pe, Visit *e) {
        using Param = Visit::Param;
        %% write exec;
        return nullptr;
    }
};

%%{
    machine location_parser;
    include json_parser;

    int_key = "id" %{ e->mask.set(Param::kId); int_val = &e->id; }
            | "distance" %{ e->mask.set(Param::kDistance); int_val = &e->distance; }
            ;
    string_key = "place" %{ e->mask.set(Param::kPlace);        string_val = &e->place; }
               | "country" %{ e->mask.set(Param::kCountry);    string_val = &e->country; }
               | "city" %{ e->mask.set(Param::kCity);          string_val = &e->city; }
               ;
    json_data = '"' int_key '"' space** ':' space** json_int
              | '"' string_key '"' space** ':' space** json_string
              ;
    main := [^\{]** '{' space** json_data (',' space** json_data space** )** '}' @{ return p + 1; };

    write data;
}%%

struct LocationParser : JsonParser {
    void reset() {
        %% write init;
    }

    const char *parse(const char *p, const char *pe, Location *e) {
        using Param = Location::Param;
        %% write exec;
        return nullptr;
    }
};


}

#endif
