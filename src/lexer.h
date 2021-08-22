#pragma once

#include <deque>
#include <iosfwd>
#include <optional>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace parse {

namespace token_type {
struct Number { // Лексема «число»
    int value;   // число
};

struct Id {            // Лексема «идентификатор»
    std::string value;  // Имя идентификатора
};

struct Char {   // Лексема «символ»
    char value;  // код символа
};

struct String { // Лексема «строковая константа»
    std::string value;
};

struct Class {};    // Лексема «class»
struct Return {};   // Лексема «return»
struct If {};       // Лексема «if»
struct Else {};     // Лексема «else»
struct Def {};      // Лексема «def»
struct Newline {};  // Лексема «конец строки»
struct Print {};    // Лексема «print»
struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
struct Dedent {};  // Лексема «уменьшение отступа»
struct Eof {};     // Лексема «конец файла»
struct And {};     // Лексема «and»
struct Or {};      // Лексема «or»
struct Not {};     // Лексема «not»
struct Eq {};      // Лексема «==»
struct NotEq {};   // Лексема «!=»
struct LessOrEq {};     // Лексема «<=»
struct GreaterOrEq {};  // Лексема «>=»
struct None {};         // Лексема «None»
struct True {};         // Лексема «True»
struct False {};        // Лексема «False»
}  // namespace token_type

// ---------- Token -------------------

using TokenBase
    = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                   token_type::Class, token_type::Return, token_type::If, token_type::Else,
                   token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                   token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
                   token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
                   token_type::None, token_type::True, token_type::False, token_type::Eof>;

struct Token : TokenBase {
    using TokenBase::TokenBase;

    template <typename T>
    [[nodiscard]] bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T* TryAs() const {
        return std::get_if<T>(this);
    }
};

bool operator==(const Token& lhs, const Token& rhs);
bool operator!=(const Token& lhs, const Token& rhs);

std::ostream& operator<<(std::ostream& os, const Token& rhs);

// ---------- Lexer -------------------

class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lexer {
    inline static const std::unordered_map<std::string, Token> keywords = {
        {"class", token_type::Class{}},
        {"return", token_type::Return{}},
        {"if", token_type::If{}},
        {"else", token_type::Else{}},
        {"def", token_type::Def{}},
        {"print", token_type::Print{}},
        {"and", token_type::And{}},
        {"or", token_type::Or{}},
        {"not", token_type::Not{}},
        {"None", token_type::None{}},
        {"True", token_type::True{}},
        {"False", token_type::False{}}
    };

    inline static const std::unordered_map<std::string, Token> comparison_lexems = {
        {"==", token_type::Eq{}},
        {"!=", token_type::NotEq{}},
        {"<=", token_type::LessOrEq{}},
        {">=", token_type::GreaterOrEq{}}
    };

public:
    explicit Lexer(std::istream& input);

    // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
    [[nodiscard]] inline const Token& CurrentToken() const {
        return tokens_.back();
    }

    // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
    Token NextToken();

    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    inline const T& Expect() const {
        if (!CurrentToken().Is<T>())
            throw LexerError("token has different type from expected");
        return CurrentToken().As<T>();
    }

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    inline void Expect(const U& value) const {
        if (const T& token = Expect<T>(); token.value != value)
            throw LexerError("token has different value from expected");
    }

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    inline const T& ExpectNext() {
        NextToken();
        return Expect<T>();
    }

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    inline void ExpectNext(const U& value) {
        NextToken();
        Expect<T>(value);
    }

private:
    std::istream& input_;
    size_t current_indent_size_ = 0;
    std::stack<uint8_t> indent_sizes_;
    std::deque<Token> tokens_;

    void MoveToNextToken();

    inline Token ParseIndent() {
        if (current_indent_size_ > indent_sizes_.top()) {
            indent_sizes_.push(current_indent_size_);
            return token_type::Indent{};
        } else {
            indent_sizes_.pop();
            return token_type::Dedent{};
        }
    }

    inline Token ParseNewLine() {
        input_.ignore();
        return token_type::Newline{};
    }

    inline Token ParseEof() const {
        return (tokens_.empty() || CurrentToken().Is<token_type::Newline>()
                                || CurrentToken().Is<token_type::Eof>()
                                || CurrentToken().Is<token_type::Dedent>())
               ? Token(token_type::Eof{})
               : Token(token_type::Newline{});

    }

    Token ParseDigit();

    Token ParseIdentifier();

    Token ParseString();

    Token ParseLexem();
};

}  // namespace parse