// https://github.com/aliakseis/json_parser

#include "json_parser.h"

#include <cctype>
#include <climits>
#include <algorithm>
#include <cassert>
#include <sstream>

#include <streambuf>


namespace {

bool error(const char* description)
{
    throw std::runtime_error(description);
}


int from_hex(char ch)
{
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

std::string unescapeString(const std::string& s)
{
    std::istringstream ss(s);
    std::string result;
    std::getline(ss, result, '\\');
    std::string buffer;

    bool doubleEscape = false;

    while (std::getline(ss, buffer, '\\'))
    {
        if (buffer.empty())
        {
            if (!doubleEscape)
            {
                result += '\\';
                doubleEscape = true;
            }
            else
                doubleEscape = false;
            continue;
        }

        if (doubleEscape)
        {
            doubleEscape = false;
            result += buffer;
            continue;
        }

        const char ch = buffer[0];
        if (ch == 'u')
        {
            buffer.size() >= 5 || error("Invalid unicode symbol");
            const int ord = (from_hex(buffer[1]) << 12) | (from_hex(buffer[2]) << 8) | (from_hex(buffer[3]) << 4) | from_hex(buffer[4]);
            // http://www.herongyang.com/Unicode/UTF-8-UTF-8-Encoding-Algorithm.html
            if (ord < 0x80) {
                result += char(ord >> 0 & 0x7F | 0x00);
            }
            else if (ord < 0x0800) {
                result += char(ord >> 6 & 0x1F | 0xC0);
                result += char(ord >> 0 & 0x3F | 0x80);
            }
            else if (ord < 0x010000) {
                result += char(ord >> 12 & 0x0F | 0xE0);
                result += char(ord >> 6 & 0x3F | 0x80);
                result += char(ord >> 0 & 0x3F | 0x80);
            }
            else if (ord < 0x110000) {
                result += char(ord >> 18 & 0x07 | 0xF0);
                result += char(ord >> 12 & 0x3F | 0x80);
                result += char(ord >> 6 & 0x3F | 0x80);
                result += char(ord >> 0 & 0x3F | 0x80);
            }
            else {
                error("Invalid unicode symbol");
            }

            result += buffer.substr(5);
        }
        else
        {
            switch (ch) {
            case  'b': result += '\b'; break;
            case  'f': result += '\f'; break;
            case  'n': result += '\n'; break;
            case  'r': result += '\r'; break;
            case  't': result += '\t'; break;
                // Pass undefined escape sequences as the escaped character.
            default: result += ch;
            }
            result += buffer.substr(1);
        }
    }
    return result;
}

template<typename T> struct Traits {};

template<> struct Traits<std::map<std::string, std::any> >
{
    enum { OPENING = '{', CLOSING = '}' };
};
template<> struct Traits<std::vector<std::any> >
{
    enum { OPENING = '[', CLOSING = ']' };
};


class JsonParser
{
public:
    JsonParser(std::istream& input);

    std::any parse(bool skipError = false);

private:
    bool scan(char ch);
    bool skip(char ch); // trims white space on both sides; for delimiters only
    void trimSpace();

    template<typename T>
    bool parseInstance(std::any& result);

    bool parseMember(std::map<std::string, std::any>& object);
    bool parseMember(std::vector<std::any>& array);

    template<typename T>
    bool parseString(T& result);
    bool parseNumber(std::any& result);
    bool parseKeyword(std::any& result);

private:
    std::istream& m_input;
}; // class JsonParser


JsonParser::JsonParser(std::istream& input)
    : m_input(input)
{
}


template<typename T>
bool JsonParser::parseInstance(std::any& result)
{
    if (!skip(Traits<T>::OPENING))
    {
        return false;
    }

    T object;
    bool moreMembers = false;
    while (parseMember(object))
    {
        moreMembers = skip(',');
        if (!moreMembers)
        {
            break;
        }
    }
    moreMembers && error("Missing member");
    skip(Traits<T>::CLOSING) || error("Unclosed instance");

    result = object;
    return true;
}

bool JsonParser::parseMember(std::map<std::string, std::any>& object)
{
    std::string key;
    if (!parseString(key))
    {
        return false;
    }
    skip(':') || error("Expecting object separator");
    object[key] = parse();
    return true;
}

bool JsonParser::parseMember(std::vector<std::any>& array)
{
    std::any value = parse(true);
    if (!value.has_value())
    {
        return false;
    }
    array.push_back(value);
    return true;
}

template<typename T>
bool JsonParser::parseString(T& result)
{
    if (!scan('"'))
    {
        return false;
    }

    std::string value;

    bool stop;
    do
    {
        std::string contents;
        std::getline(m_input, contents, '"') || error("Unclosed string");
        const auto begin = contents.rbegin();
        const auto cnt 
            = std::find_if(begin, contents.rend(), [](char ch) { return ch != '\\'; }) - begin;
        stop = !(cnt & 1);
        if (!stop)
            contents[contents.size() - 1] = '"';
        value += contents;
    } while (!stop);

    result = unescapeString(value);
    return true;
}

std::any JsonParser::parse(bool skipError)
{
    std::any result;
    trimSpace();

    parseInstance<std::map<std::string, std::any> >(result) // object
        || parseInstance<std::vector<std::any> >(result) // array
        || parseString(result)
        || parseKeyword(result)
        || parseNumber(result)
        || skipError
        || error("Illegal JSON value");

    trimSpace();
    return result;
}


bool JsonParser::scan(char ch)
{
    if (!m_input)
        return false;

    if (m_input.peek() == ch)
    {
        m_input.get();
        return true;
    }

    return false;
}

bool JsonParser::skip(char ch)
{
    trimSpace();
    if (scan(ch))
    {
        trimSpace();
        return true;
    }
    return false;
}


void JsonParser::trimSpace()
{
    while (m_input && std::isspace(m_input.peek()))
        m_input.get();
}

bool JsonParser::parseNumber(std::any& result)
{
    try
    {
        double value{};
        if (!(m_input >> value))
        {
            m_input.clear();
            return false;
        }
        result = value;
    }
    catch (const std::exception&)
    {
        return false;
    }
    return true;
}

bool JsonParser::parseKeyword(std::any& result)
{
    if (scan('t'))
    {
        if (scan('r') && scan('u') && scan('e'))
        {
            result = true;
            return true;
        }
    }
    else if (scan('f'))
    {
        if (scan('a') && scan('l') && scan('s') && scan('e'))
        {
            result = false;
            return true;
        }
    }
    else if (scan('n'))
    {
        if (scan('u') && scan('l') && scan('l'))
        {
            result.reset();
            return true;
        }
    }
    else
    {
        return false;
    }

    error("Unexpected keyword");
    return false;
}


class ByteStreamBuffer : public std::streambuf
{
public:
    ByteStreamBuffer(char* base, size_t length)
    {
        setg(base, base, base + length);
    }

protected:
    pos_type seekoff(off_type offset,
        std::ios_base::seekdir dir,
        std::ios_base::openmode) override
    {
        char* whence = eback();
        if (dir == std::ios_base::cur)
        {
            whence = gptr();
        }
        else if (dir == std::ios_base::end)
        {
            whence = egptr();
        }
        char* to = whence + offset;

        // check limits
        if (to >= eback() && to <= egptr())
        {
            setg(eback(), to, egptr());
            return gptr() - eback();
        }

        return -1;
    }
};

} // namespace


std::any parseJson(std::istream& input, bool skipError /*= false*/)
{
    return JsonParser(input).parse(skipError);
}

std::any parseJson(const std::string_view& input, bool skipError /*= false*/)
{
    ByteStreamBuffer buf(const_cast<char*>(input.data()), input.length());
    std::istream is(&buf);
    return parseJson(is, skipError);
}
