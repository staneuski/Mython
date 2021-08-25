#include "lexer.h"

#include <algorithm>
#include <charconv>

using namespace std;

namespace parse {
// ---------- Token -------------------

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index())
        return false;
    else if (lhs.Is<Char>())
        return lhs.As<Char>().value == rhs.As<Char>().value;
    else if (lhs.Is<Number>())
        return lhs.As<Number>().value == rhs.As<Number>().value;
    else if (lhs.Is<String>())
        return lhs.As<String>().value == rhs.As<String>().value;
    else if (lhs.Is<Id>())
        return lhs.As<Id>().value == rhs.As<Id>().value;
    else
        return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

// ---------- Lexer -------------------

Lexer::Lexer(std::istream& input) : input_(input) {
    indent_sizes_.push(current_indent_size_);
    NextToken();
}

Token Lexer::NextToken() {
    MoveToNextToken();

    const char c = input_.peek();
    if (current_indent_size_ != indent_sizes_.top()) {
        tokens_.push_back(ParseIndent());
    } else if (std::isdigit(c)) {
        tokens_.push_back(ParseDigit());
    } else if (std::isalpha(c) || c == '_') {
        tokens_.push_back(ParseIdentifier());
    } else if (c == '\'' || c == '\"') {
        tokens_.push_back(ParseString());
    } else if (c == '!' || c == '<' || c == '>' || c == '=') {
        tokens_.push_back(ParseLexem());
    } else if (c == '\n') {
        tokens_.push_back(ParseNewLine());
    } else if (c == EOF) {
        tokens_.push_back(ParseEof());
    } else {
        tokens_.push_back(token_type::Char{c});
        input_.ignore();
    }

    return tokens_.back();
}

void Lexer::MoveToNextToken() {
    const bool is_new_line = !tokens_.empty()
                             && CurrentToken().Is<token_type::Newline>();

    size_t space_count;
    while (true) {
        // Count and ignore spaces
        for (space_count = 0; input_.peek() == ' '; ++space_count)
            input_.ignore();

        // Ignore comment line
        if (input_.peek() == '#')
            while (input_.peek() != '\n' && input_.peek() != EOF)
                input_.ignore();

        if (input_.peek() == '\n' && (tokens_.empty() || is_new_line))
            input_.ignore();
        else
            break;
    }

    if (is_new_line && space_count % 2 != 0)
        throw LexerError("indent size must be even");
    else if (is_new_line && space_count != indent_sizes_.top())
        current_indent_size_ = space_count;
}

Token Lexer::ParseDigit() {
    std::stringstream ss;
    while (std::isdigit(input_.peek()))
        ss << static_cast<char>(input_.get());
    return token_type::Number{std::stoi(ss.str())};
}

Token Lexer::ParseIdentifier() {
    std::string identifier;
    for (char c = input_.peek();
         std::isalpha(c) || std::isdigit(c) || c == '_';
         c = input_.peek())
        identifier.push_back(input_.get());

    return (keywords.find(identifier) != keywords.end()) 
           ? keywords.at(identifier)
           : Token(token_type::Id{identifier});
}

Token Lexer::ParseString() {
    std::string str;

    const char quote = input_.get();
    for (char curr = input_.get(); curr != quote; curr = input_.get())
        if (curr == '\\')
            switch (const char next = input_.get(); next) {
            case '\'':
                [[fallthrough]];
            case '\"':
                str.push_back(next);
                break;
            case 'n':
                str.push_back('\n');
                break;
            case 't':
                str.push_back('\t');
                break;
            default:
                str.push_back(curr);
                str.push_back(next);
                break;
            }
        else
            str.push_back(curr);

    return token_type::String{str};
}

Token Lexer::ParseLexem() {
    const char c = input_.get();
    if (input_.peek() == '=') {
        std::string str{c};
        str.push_back(input_.get());
        return comparison_lexems.at(str);
    } else {
        return token_type::Char{c};
    }
}

}  // namespace parse