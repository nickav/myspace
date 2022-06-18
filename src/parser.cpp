#pragma once

typedef u32 Token_Type;
enum {
    TokenType_Unknown,

    TokenType_Number,
    TokenType_Literal,
    TokenType_Paren,
    TokenType_Brace,
    TokenType_Bracket,
    TokenType_Quote,
    TokenType_Operator,
    TokenType_Semicolon,
    TokenType_Comma,
    TokenType_Equals,
    TokenType_Comment,
    TokenType_Whitespace,

    TokenType_COUNT,
};

struct Token {
    Token_Type type;
    String     value;
};

Array<Token> tokenize(String text)
{
    Array<Token> tokens = {};
    tokens.allocator = temp_allocator();

    i64 i = 0;
    while (i < text.count)
    {
        char it = text[i];

        // Whitespace
        if (char_is_whitespace(it))
        {
            i64 start = i;
            while (i < text.count && char_is_whitespace(text[i])) {
                i ++;
            }

            auto token = array_push(&tokens);
            token->type = TokenType_Whitespace;
            token->value = string_slice(text, start, i);
            continue;
        }

        // Comments
        if (it == '/' && (i + 1) < text.count)
        {
            // Single-line comment
            if (text.data[i + 1] == '/')
            {
                i64 start = i;

                i += 2; // skip the //
                while (i < text.count && text[i] != '\n')
                {
                    i ++;
                }

                auto token = array_push(&tokens);
                token->type = TokenType_Comment;
                token->value = string_slice(text, start, i);
                continue;
            }

            if (text.data[i + 1] == '*')
            {
                i64 start = i;
                auto at = string_slice(text, i + 2);
                i64 scope_depth = 1;
                while (at.count > 0 && scope_depth > 0)
                {
                    if (string_starts_with(at, S("/*"))) {
                        scope_depth ++;
                        i += 2;
                        continue;
                    }

                    if (string_starts_with(at, S("*/"))) {
                        scope_depth --;
                        i += 2;
                        continue;
                    }

                    string_advance(&at, 1);
                    i += 1;
                }

                auto value = string_slice(text, start, i + 2);

                auto token = array_push(&tokens);
                token->type = TokenType_Comment;
                token->value = value;
                continue;
            }
        }

        // Strings
        if (it == '"' || it == '\'')
        {
        }

        // Literals
        if (char_is_alpha(it))
        {
            i64 start = i;
            while (i < text.count && (char_is_alpha(text[i]) || char_is_numeric(text[i]))) {
                i ++;
            }

            auto token = array_push(&tokens);
            token->type = TokenType_Comment;
            token->value = string_slice(text, start, i);
            continue;
        }

        i ++;
    }


    return tokens;
}

