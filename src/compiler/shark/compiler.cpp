#include "compiler.h"

#include <iostream>
#include <map>
#include <new>
#include <utility>

namespace Shark {
    using std::cout;
    using std::vector;
    using std::map;
    using std::string;
    using std::unique_ptr;
    using std::move;

    using Intermediate::Program;

    void stringPushChar32(string &str, char32_t c) {
        if (c <= 0x7f) {
            str.push_back(static_cast<char>(c));
        } else if (c <= 0x07ff) {
            str.append({
                static_cast<char>(0xc0 | (c >> 6)),
                static_cast<char>(0x80 | (c & 0x3f)),
            });
        } else if (c <= 0xffff) {
            str.append({
                static_cast<char>(0xe0 | (c >> 12)),
                static_cast<char>(0x80 | ((c >> 6) & 0x3f)),
                static_cast<char>(0x80 | (c & 0x3f)),
            });
        } else {
            str.append({
                static_cast<char>(0xf0 | (c >> 18)),
                static_cast<char>(0x80 | ((c >> 12) & 0x3f)),
                static_cast<char>(0x80 | ((c >> 6) & 0x3f)),
                static_cast<char>(0x80 | (c & 0x3f)),
            });
        }
    }

    struct InvalidByteError {
        const SourceFile &file;
        size_t index;

        InvalidByteError(const SourceFile &file, size_t index)
            : file(file), index(index) {}
    };

    struct SourcePosition {
        const SourceFile &file;
        size_t line;
        size_t column;

        SourcePosition(const SourceFile &file, size_t line, size_t column)
            : file(file), line(line), column(column) {}
    };

    enum class SyntaxErrorKind: uint32_t {
        CANNOT_END_HERE,
        CANNOT_BE_HERE
    };

    struct SyntaxError {
        SyntaxErrorKind kind;
        vector<string> arguments;
        SourcePosition position;

        SyntaxError(SyntaxErrorKind kind, vector<string> arguments, SourcePosition position)
            : kind(kind), arguments(move(arguments)), position(position) {}
    };

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

        void setNone() {
            destroy();
            kind = TokenKind::NONE;
        }
        void setEnd() {
            destroy();
            kind = TokenKind::END;
        }
        void setFunction() {
            destroy();
            kind = TokenKind::FUNCTION;
        }
        void setLeftParenthesis() {
            destroy();
            kind = TokenKind::LEFT_PARENTHESIS;
        }
        void setRightParenthesis() {
            destroy();
            kind = TokenKind::RIGHT_PARENTHESIS;
        }
        void setLeftBrace() {
            destroy();
            kind = TokenKind::LEFT_BRACE;
        }
        void setRightBrace() {
            destroy();
            kind = TokenKind::RIGHT_BRACE;
        }
        void setSemicolon() {
            destroy();
            kind = TokenKind::SEMICOLON;
        }
        void setIdentifier(string name) {
            destroy();
            kind = TokenKind::IDENTIFIER;
            new (&data.str) string(move(name));
        }
        void setInteger(string valueStr) {
            destroy();
            kind = TokenKind::INTEGER;
            new (&data.str) string(move(valueStr));
        }

        Token(const Token &other) = delete;
        Token(Token &&other) = delete;

        Token& operator=(const Token &other) = delete;
        Token& operator=(Token &&other) = delete;

        ~Token() {
            destroy();
        }

        TokenKind getKind() const noexcept { return kind; }

        bool isNone() { return kind == TokenKind::NONE; }
        bool isEnd() { return kind == TokenKind::END; }
        bool isFunction() { return kind == TokenKind::FUNCTION; }
        bool isLeftParenthesis() { return kind == TokenKind::LEFT_PARENTHESIS; }
        bool isRightParenthesis() { return kind == TokenKind::RIGHT_PARENTHESIS; }
        bool isLeftBrace() { return kind == TokenKind::LEFT_BRACE; }
        bool isRightBrace() { return kind == TokenKind::RIGHT_BRACE; }
        bool isSemicolon() { return kind == TokenKind::SEMICOLON; }
        bool isIdentifier() { return kind == TokenKind::IDENTIFIER; }
        bool isInteger() { return kind == TokenKind::INTEGER; }

        bool takeIdentifier(string &outName) {
            if (kind == TokenKind::IDENTIFIER) {
                outName = move(data.str);
                return true;
            }
            return false;
        }
        bool takeInteger(string &outValueStr) {
            if (kind == TokenKind::INTEGER) {
                outValueStr = move(data.str);
                return true;
            }
            return false;
        }

    private:
        TokenKind kind;
        union Data {
            string str;
            Data() {}
            ~Data() {}
        } data;

        void destroy() noexcept {
            if (kind == TokenKind::IDENTIFIER || kind == TokenKind::INTEGER) {
                data.str.~basic_string();
            }
        }
    };

    static map<string, void (Token::*)()> keywordSetters{
        { "function", Token::setFunction }
    };

    namespace Expression {
        class Base {
        public:
            virtual ~Base() {}
        };

        class Integer: public Base {
        public:
            Integer(SourcePosition position, string valueStr)
                : position(position), valueStr(move(valueStr)) { }
        private:
            SourcePosition position;
            string valueStr;
        };
    }

    namespace Statement {
        class Base {
        public:
            virtual ~Base() {}
        };

        class Block: public Base {
        public:
            Block(SourcePosition position, vector<unique_ptr<Base> > statements)
                : position(position), statements(move(statements)) { }
        private:
            SourcePosition position;
            vector<unique_ptr<Base> > statements;
        };

        class Expression: public Base {
        public:
            Expression(SourcePosition position, unique_ptr<Shark::Expression::Base> &&expression)
                : position(position), expression(move(expression)) { }
        private:
            SourcePosition position;
            unique_ptr<Shark::Expression::Base> expression;
        };
    }

    namespace Item {
        class Base {
        public:
            virtual ~Base() {}
        };

        class Function: public Base {
        public:
            Function(SourcePosition position, string name, unique_ptr<Statement::Block> &&body)
                : position(position), name(move(name)), body(move(body)) { }
        private:
            SourcePosition position;
            string name;
            unique_ptr<Statement::Block> body;
        };
    }

    class FileNode {
        vector<unique_ptr<Item::Base> > items;
    public:
        FileNode(vector<unique_ptr<Item::Base> > items)
            : items(move(items)) { }
    };

    class Parser {
        const SourceFile &file;
        size_t index{0};
        bool byteEnd{false};
        size_t byteOffset{0};
        unsigned char b{0};
        bool charEnd{false};
        size_t charLine{1};
        size_t charColumn{1};
        char32_t c{U'\0'};
        size_t tokenLine{1};
        size_t tokenColumn{1};
        Token token;
    public:
        Parser(const SourceFile &file)
            : file(file) {
            token.setEnd();
        }

        InvalidByteError invalidByte() {
            return InvalidByteError{file, byteOffset};
        }

        SyntaxError charCannotBeHere() {
            if (!charEnd) {
                string str;
                stringPushChar32(str, c);
                return SyntaxError{SyntaxErrorKind::CANNOT_BE_HERE, {move(str)}, SourcePosition{file, charLine, charColumn}};
            }
            return SyntaxError{SyntaxErrorKind::CANNOT_END_HERE, {}, SourcePosition{file, charLine, charColumn}};
        }

        SyntaxError tokenCannotBeHere() {
            if (!token.isEnd()) {
                return SyntaxError{SyntaxErrorKind::CANNOT_BE_HERE, {}, SourcePosition{file, tokenLine, tokenColumn}};
            }
            return SyntaxError{SyntaxErrorKind::CANNOT_END_HERE, {}, SourcePosition{file, tokenLine, tokenColumn}};
        }

        void nextByte() {
            if (!byteEnd) {
                if (index < file.content.length()) {
                    b = static_cast<unsigned char>(file.content[index]);
                    ++index;
                } else {
                    byteEnd = true;
                }
            } else {
                throw invalidByte();
            }
        }

        unsigned char getUtf8Continuation() {
            if ((b & 0xc0) == 0x80) {
                return b & 0x3f;
            }
            throw invalidByte();
        }

        void nextChar() {
            if (charEnd) {
                throw charCannotBeHere();
            }
            if (byteEnd) {
                charEnd = true;
                return;
            }

            if ((b & 0x80) == 0) {
                // 0xxxxxxx
                c = b;
                nextByte();
            } else if ((b & 0xe0) == 0xc0) {
                // 110xxxxx
                if (b < 0xc2) {
                    throw invalidByte();
                }

                c = (b & 0x1f) << 6;
                nextByte();

                c |= getUtf8Continuation();
                nextByte();
            } else if ((b & 0xf0) == 0xe0) {
                // 1110xxxx
                unsigned char first = (b & 0xf);
                c = first << 12;
                nextByte();

                unsigned char second = getUtf8Continuation();
                if ((first == 0 && second < 0x20) || (first == 0xd && second > 0x1f)) {
                    throw invalidByte();
                }
                c |= second << 6;
                nextByte();

                c |= getUtf8Continuation();
                nextByte();
            } else if ((b & 0xf8) == 0xf0) {
                // 11110xxx
                if (b > 0xf4) {
                    throw invalidByte();
                }
                unsigned char first = b & 0x7;
                c = first << 18;
                nextByte();

                unsigned char second = getUtf8Continuation();
                if ((first == 0 && second < 0x10) || (first == 0x4 && second > 0xf)) {
                    throw invalidByte();
                }
                c |= second << 12;
                nextByte();

                c |= getUtf8Continuation() << 6;
                nextByte();

                c |= getUtf8Continuation();
                nextByte();
            } else {
                throw charCannotBeHere();
            }
        }

        bool charIsSpace() {
            return !charEnd && isspace(c);
        }

        bool charIsAlpha() {
            return !charEnd && isalpha(c);
        }

        bool charIsDigit() {
            return !charEnd && isdigit(c);
        }

        bool charEqual(char32_t other) {
            return !charEnd && c == other;
        }

        void nextToken() {
            while (charIsSpace()) {
                nextChar();
            }

            if (charIsAlpha() || charEqual(U'_')) {
                string name;
                stringPushChar32(name, c);
                nextChar();

                while (charIsAlpha() || charIsDigit() || charEqual(U'_')) {
                    stringPushChar32(name, c);
                    nextChar();
                }

                auto setterEntry = keywordSetters.find(name);
                if (setterEntry != keywordSetters.cend()) {
                    (token.*setterEntry->second)();
                } else {
                    token.setIdentifier(move(name));
                }
            } else if (charIsDigit()) {
                string valueStr;
                stringPushChar32(valueStr, c);
                nextChar();

                while (charIsDigit()) {
                    stringPushChar32(valueStr, c);
                    nextChar();
                }

                token.setInteger(move(valueStr));
            } else if (charEqual(U'(')) {
                token.setLeftParenthesis();
                nextChar();
            } else if (charEqual(U')')) {
                token.setRightParenthesis();
                nextChar();
            } else if (charEqual(U'{')) {
                token.setLeftBrace();
                nextChar();
            } else if (charEqual(U'}')) {
                token.setRightBrace();
                nextChar();
            } else if (charEqual(U';')) {
                token.setSemicolon();
                nextChar();
            } else if (charEnd) {
                token.setEnd();
            } else {
                throw charCannotBeHere();
            }
        }

        void assertAndNextToken(TokenKind kind) {
            if (token.getKind() != kind) {
                throw tokenCannotBeHere();
            }
            nextToken();
        }

        SourcePosition getTokenPosition() {
            return SourcePosition{file, tokenLine, tokenColumn};
        }

        bool isExpressionStart() {
            return token.isInteger();
        }

        unique_ptr<Expression::Base> parseExpression(int priority) {
            SourcePosition position = getTokenPosition();
            string valueStr;
            if (!token.takeInteger(valueStr)) {
                throw tokenCannotBeHere();
            }
            nextToken();

            return unique_ptr<Expression::Base>{new Expression::Integer{position, move(valueStr)}};
        }

        bool isBlockStatementStart() {
            return token.isLeftBrace();
        }

        unique_ptr<Statement::Block> parseBlockStatement() {
            SourcePosition position = getTokenPosition();
            assertAndNextToken(TokenKind::LEFT_BRACE);

            vector<unique_ptr<Statement::Base>> statements;
            for (;;) {
                if (token.isRightBrace()) {
                    nextToken();
                    break;
                }

                statements.push_back(parseStatement());
            }

            return unique_ptr<Statement::Block>{new Statement::Block{position, move(statements)}};
        }

        unique_ptr<Statement::Base> parseStatement() {
            SourcePosition position = getTokenPosition();

            if (isBlockStatementStart()) {
                return parseBlockStatement();
            } else if (isExpressionStart()) {
                unique_ptr<Expression::Base> expression = parseExpression(0);
                assertAndNextToken(TokenKind::SEMICOLON);
                return unique_ptr<Statement::Expression>{new Statement::Expression{position, move(expression)}};
            } else {
                throw tokenCannotBeHere();
            }
        }

        bool isItemStart() {
            return token.isFunction();
        }

        unique_ptr<Item::Base> parseItem() {
            SourcePosition position = getTokenPosition();

            if (token.isFunction()) {
                nextToken();

                string name;
                if (!token.takeIdentifier(name)) {
                    throw tokenCannotBeHere();
                }
                nextToken();

                assertAndNextToken(TokenKind::LEFT_PARENTHESIS);
                assertAndNextToken(TokenKind::RIGHT_PARENTHESIS);

                unique_ptr<Statement::Block> body = parseBlockStatement();

                return unique_ptr<Item::Base>{new Item::Function{position, move(name), move(body)}};
            } else {
                throw tokenCannotBeHere();
            }
        }

        unique_ptr<FileNode> parse() {
            nextByte();
            nextChar();
            nextToken();
            vector<unique_ptr<Item::Base>> items;
            for (;;) {
                if (isItemStart()) {
                    items.push_back(parseItem());
                } else {
                    break;
                }
            }

            return unique_ptr<FileNode>{new FileNode{move(items)}};
        }
    };

    unique_ptr<Program> compile(const vector<SourceFile> &files) {
        vector<unique_ptr<FileNode> > nodes;
        nodes.reserve(files.size());

        for (const SourceFile &file: files) {
            Parser parser{file};
            nodes.push_back(parser.parse());
        }

        return unique_ptr<Program>{new Program{}};
    }
}
