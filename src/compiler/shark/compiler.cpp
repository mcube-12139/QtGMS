#include "compiler.h"

#include <map>
#include <functional>
#include <new>
#include <utility>

namespace Shark {
    using std::vector;
    using std::map;
    using std::u16string;
    using std::function;
    using std::unique_ptr;
    using std::move;

    using Intermediate::Program;

    enum class TokenKind: uint32_t {
        NONE,
        END,
        FUNCTION,
        LEFT_PARENTHESIS,
        RIGHT_PARENTHESIS,
        LEFT_BRACE,
        RIGHT_BRACE,
        SEMICOLON,
        IDENTIFIER,
        INTEGER,
    };

    struct Token {
        Token() noexcept: kind(TokenKind::NONE) { }

        static Token makeEnd() {
            Token token;
            token.kind = TokenKind::END;
            return token;
        }
        static Token makeFunction() {
            Token token;
            token.kind = TokenKind::FUNCTION;
            return token;
        }
        static Token makeLeftParenthesis() {
            Token token;
            token.kind = TokenKind::LEFT_PARENTHESIS;
            return token;
        }
        static Token makeRightParenthesis() {
            Token token;
            token.kind = TokenKind::RIGHT_PARENTHESIS;
            return token;
        }
        static Token makeLeftBrace() {
            Token token;
            token.kind = TokenKind::LEFT_BRACE;
            return token;
        }
        static Token makeRightBrace() {
            Token token;
            token.kind = TokenKind::RIGHT_BRACE;
            return token;
        }
        static Token makeSemicolon() {
            Token token;
            token.kind = TokenKind::SEMICOLON;
            return token;
        }
        static Token makeIdentifier(u16string name) {
            Token token;
            token.kind = TokenKind::IDENTIFIER;
            new (&token.data.str) u16string(move(name));
            return token;
        }
        static Token makeInteger(u16string valueStr) {
            Token token;
            token.kind = TokenKind::INTEGER;
            new (&token.data.str) u16string(move(valueStr));
            return token;
        }

        Token(const Token &other) = delete;

        Token(Token &&other) noexcept: kind(other.kind) {
            if (
                other.kind == TokenKind::IDENTIFIER
                || other.kind == TokenKind::INTEGER
            ) {
                new (&data.str) u16string(move(other.data.str));
            }
        }

        Token& operator=(const Token &other) = delete;

        Token& operator=(Token &&other) noexcept {
            if (this != &other) {
                destroy();
                kind = other.kind;
                if (
                    other.kind == TokenKind::IDENTIFIER
                    || other.kind == TokenKind::INTEGER
                ) {
                    new (&data.str) u16string(move(other.data.str));
                }
            }
            return *this;
        }

        ~Token() {
            destroy();
        }

        TokenKind getKind() const noexcept { return kind; }

        bool isEnd() { return kind == TokenKind::END; }
    private:
        TokenKind kind;
        union Data {
            u16string str;
            Data() {}
            ~Data() {}
        } data;

        void destroy() noexcept {
            if (kind == TokenKind::IDENTIFIER || kind == TokenKind::INTEGER) {
                data.str.~basic_string();
            }
            kind = TokenKind::NONE;
        }
    };

    static map<u16string, function<Token()> > keywordConstructors{
        { u"function", Token::makeFunction }
    };

    class FileNode {

    };

    class Parser {
        const SourceFile &file;
        size_t index{0};
        char16_t c{u'\0'};
        Token token = Token::makeEnd();
    public:
        Parser(const SourceFile &file)
            : file(file) { }

        void nextChar() {
            if (index < file.content.length()) {
                c = file.content[index];
                ++index;
            } else {
                c = u'\0';
            }
        }

        void nextToken() {
            while (isspace(c)) {
                nextChar();
            }

            if (isalpha(c) || c == u'_') {
                u16string name{c};
                nextChar();
                while (isalpha(c) || isdigit(c) || c == u'_') {
                    name.push_back(c);
                    nextChar();
                }

                auto constructor = keywordConstructors.find(name);
                if (constructor != keywordConstructors.cend()) {
                    token = constructor->second();
                } else {
                    token = Token::makeIdentifier(move(name));
                }
            } else if (c == u'(') {
                token = Token::makeLeftParenthesis();
                nextChar();
            } else if (c == u')') {
                token = Token::makeRightParenthesis();
                nextChar();
            } else if (c == u'{') {
                token = Token::makeLeftBrace();
                nextChar();
            } else if (c == u'}') {
                token = Token::makeRightBrace();
                nextChar();
            } else if (c == u';') {
                token = Token::makeSemicolon();
                nextChar();
            } else if (c == u'\0') {
                token = Token::makeEnd();
                nextChar();
            } else {
                throw u"wtf";
            }
        }

        unique_ptr<FileNode> parse() {
            nextChar();
            nextToken();
            while (!token.isEnd()) {
                nextToken();
            }

            return unique_ptr<FileNode>{new FileNode{}};
        }
    };

    unique_ptr<Program> compile(const vector<SourceFile> &files) {
        for (const SourceFile &file: files) {
            Parser parser{file};
            parser.parse();
        }
        return unique_ptr<Program>{new Program{}};
    }
}
