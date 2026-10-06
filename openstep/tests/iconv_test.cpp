/* Exercise the real libiconv bridge, including 16-bit strings on old libstdc++. */
#include "dosbox.h"
#include "iconvpp.hpp"
#include <cassert>
#include <cstring>

int main()
{
    typedef _Iconv<char, uint16_t> ToUTF16;
    typedef _Iconv<uint16_t, char> FromUTF16;
    ToUTF16 *to = ToUTF16::create("UTF-8");
    FromUTF16 *from = FromUTF16::create("UTF-8");
    assert(to && from);
    const std::string text("A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");
    std::basic_string<uint16_t> wide;
    assert(to->string_convert(wide, text) >= 0);
    assert(wide.size() == 5);
    assert(wide[0] == 'A' && wide[1] == 0xe9 && wide[2] == 0x20ac);
    assert(wide[3] == 0xd83d && wide[4] == 0xde00);
    std::string back;
    assert(from->string_convert(back, wide) >= 0 && back == text);

    uint16_t out[3] = {0xffff, 0xffff, 0xffff};
    to->set_dest(out, 3);
    to->set_src("Hi");
    assert(to->string_convert() >= 0);
    assert(out[0] == 'H' && out[1] == 'i' && out[2] == 0);
    assert(to->get_src_last_read() == 2 && to->get_dest_last_written() == 2);
    to->finish();
    assert(to->string_convert() == ToUTF16::err_notvalid);

    to->set_dest(out, 1);
    to->set_src("AB");
    assert(to->string_convert() == ToUTF16::err_noroom);
    assert(to->get_src_last_read() == 1);
    to->set_dest(out, 3);
    to->set_src("\xff");
    assert(to->string_convert() == ToUTF16::err_notvalid);
    to->set_dest(out, 3);
    to->set_src("\xe2\x82");
    assert(to->string_convert() == ToUTF16::err_incomplete);
    assert(ToUTF16::create("DOSBOX-NO-SUCH-ENCODING") == NULL);
    delete to;
    delete from;
    puts("OPENSTEP iconv tests passed");
    return 0;
}
