#ifndef HLCUP_HTTPPARSER_HPP__
#define HLCUP_HTTPPARSER_HPP__

#include <string>

#include "common.hpp"
#include "Request.hpp"
#include "json_parser.hpp"

namespace hlcup {

%%{
    machine http_parser;

    crlf = '\r\n';
    skip_to_clrf = [^\r]* crlf;
    action add_digit { tmp.int_val = tmp.int_val * 10 + (fc - '0'); }
    int_number = ([0-9] @add_digit)+ >{tmp.int_val = 0;};

    proto = ('HTTP/1.1' | 'HTTP/1.0');

    entity_id = ([0-9] @{ r.entity_id = r.entity_id * 10 + (fc - '0'); })+ >{ r.entity_id = 0; }
              ;

    action uri_param_err {
        r.action = Request::kBadRequest;
        fgoto headers;
    }

    skip_request := [^\n]** '\n';

    uri_int_param = [0-9] @{ *param_int = fc - '0'; } ([0-9] @{ *param_int = *param_int * 10 + (fc - '0'); } | [^0-9& ] @uri_param_err)** 
                  | '-' [0-9] @{ *param_int = fc - '0'; } ([0-9] @{ *param_int = *param_int * 10 + (fc - '0'); } | [^0-9& ] @uri_param_err)** %{ *param_int = -*param_int; }
                  | [^0-9\-] @uri_param_err
                  | '-' [^0-9] @uri_param_err
                  ;

    uri_string_param_char = '%' ([0-9] @{ tmp_ch  = (fc - '0') << 4; } | [a-f] @{ tmp_ch = (fc - 'a' + 10) << 4; } | [A-F] @{ tmp_ch = (fc - 'A' + 10) << 4; })
                                ([0-9] @{ tmp_ch |= (fc - '0') ; } | [a-f] @{ tmp_ch |= (fc - 'a' + 10); } | [A-F] @{ tmp_ch |= (fc - 'A' + 10); })
                                %{ *param_string += tmp_ch; }
                          | '+' @{ *param_string += ' '; }
                          | [^%&+ ] @{ *param_string += fc; }
                          ;

    uri_string_param = uri_string_param_char** >{ param_string->clear(); };

    uri_int_param_key = 'fromDate' %{ param_int = &r.from_date; r.mask.set(Param::kFromDate); } 
                      | 'toDate' %{ param_int = &r.to_date; r.mask.set(Param::kToDate); } 
                      | 'toDistance' %{ param_int = &r.to_distance; r.mask.set(Param::kToDistance); }
                      | 'fromAge' %{ param_int = &r.from_age; r.mask.set(Param::kFromAge); }
                      | 'toAge' %{ param_int = &r.to_age; r.mask.set(Param::kToAge); }
                      ;
    uri_string_param_key = 'country' %{ param_string = &r.country; r.mask.set(Param::kCountry); }
                         ;

    uri_param = uri_int_param_key '=' uri_int_param 
              | uri_string_param_key '=' uri_string_param
              | 'gender=' ( 'm' %{ r.mask.set(Param::kGender); r.gender = Gender::kMale; } 
                          | 'f' %{ r.mask.set(Param::kGender); r.gender = Gender::kFemale; }
                          | ([mf] [^ &]) @uri_param_err 
                          | ([^mf &]) @uri_param_err 
                          )
              | ((([^ &=]** -- uri_int_param_key) -- uri_string_param_key) -- 'gender') ('=' [^ &]**)?
              ;
              
    uri_params = uri_param ('&' uri_param?)**;

    maybe_uri_params = ('?' uri_params)? ' ';
    maybe_entity_id = (entity_id | [^0-9 ?/]** @{ r.entity_id = Entity::kInvalidId; });

    request = 'GET /users/' maybe_entity_id (maybe_uri_params       %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kGetUser; } 
                                      | '/visits' maybe_uri_params  %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kGetUserVisits; })
            | 'GET /visits/' maybe_entity_id maybe_uri_params       %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kGetVisit; }
            | 'GET /locations/' maybe_entity_id (maybe_uri_params   %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kGetLocation; }
                                          | '/avg' maybe_uri_params %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kGetLocationAvg; })
            | 'POST /users/' ((maybe_entity_id -- 'new')  maybe_uri_params      %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kPostUser; up.reset(); r.u.mask.reset(); }
                             | 'new' maybe_uri_params                           %{ r.action = Action::kPostUserNew; up.reset(); r.u.mask.reset(); }
                             )
            | 'POST /locations/' ((maybe_entity_id -- 'new')  maybe_uri_params  %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kPostLocation; lp.reset(); r.l.mask.reset(); }
                             | 'new' maybe_uri_params                           %{ r.action = Action::kPostLocationNew; lp.reset(); r.l.mask.reset(); }
                             )
            | 'POST /visits/' ((maybe_entity_id -- 'new')  maybe_uri_params     %{ if (r.entity_id != Entity::kInvalidId) r.action = Action::kPostVisit; vp.reset(); r.v.mask.reset(); }
                             | 'new' maybe_uri_params                           %{ r.action = Action::kPostVisitNew; vp.reset(); r.v.mask.reset(); }
                             )
            | 'GET /' maybe_uri_params %{ r.action = Action::kGetIndex; }
            ;
            
    h_ws = [ \t];
    content_length = ([0-9] @{ content_length = content_length * 10 + (fc - '0'); })+ >{ content_length = 0; };
    header = /content-length/i h_ws** ':' h_ws** content_length crlf
           | /connection/i h_ws** ':' h_ws** /close/i %{ connection_close = true; } crlf
           | [^\r\n] [^\n]** '\n'
           ;

    headers := header** '\r' '\n' @{ headers_done = true; if (content_length == 0) done = true; };
    request_line_end := proto crlf @{ fgoto headers; } ;

    main := space** (request | !request %{ r.action = Action::kNotFound; }) proto crlf @{ fgoto headers; };
}%%

%% write data;

struct HttpParser {
    using Action = Request::Action;
    using Param = Request::Param;

    int32_t *param_int;
    std::string *param_string;

    int cs;
    unsigned int content_length = 0;
    char tmp_ch;
    bool connection_close = false;
    bool done = false;
    bool headers_done = false;

    UserParser up;
    LocationParser lp;
    VisitParser vp;

    Request r;

    HttpParser() {
        reset();
    }

    void reset() {
        connection_close = headers_done = done = false;
        content_length = 0;
        %% write init;
        r.reset();
    }

    const char *parse_body(const char *p, const char *pe) {
        const char *body_end = std::min(pe, p + content_length);
        if (body_end == p) {
            return p;
        }
        content_length -= body_end - p;

        if (r.action == Action::kPostUser || r.action == Action::kPostUserNew) {
            p = up.parse(p, body_end, &r.u);
        } else if (r.action == Action::kPostLocation || r.action == Action::kPostLocationNew) {
            p = lp.parse(p, body_end, &r.l);
        } else if (r.action == Action::kPostVisit || r.action == Action::kPostVisitNew) {
            p = vp.parse(p, body_end, &r.v);
        }

        if (p == nullptr) {
            r.action = Action::kBadRequest;
        }

        if (content_length == 0) {
            done = true;
        }

        return body_end;
    }

    const char *parse(const char *p, const char *pe) {
        if (!headers_done) {
            %% write exec;
        }
        if (headers_done) {
            if (content_length != 0) {
                return parse_body(p, pe);
            } else if (r.action > Request::kGetLast) {
                r.action = Action::kBadRequest;
            }
        }
        return p;
    }

    const char *parse(const std::string &data) {
        return parse(data.data(), data.data() + data.size());
    }
};

}

#endif