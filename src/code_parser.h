#pragma once

// Here is a single line comment
/*
Here is a block comment!
*/

typedef u32 C_Token_Type;
typedef u32 C_Parser_Type;
enum {
    C_TokenType_Unknown = 0,

    C_TokenType_Number,
    C_TokenType_String,
    C_TokenType_Literal,
    C_TokenType_Identifier,

    C_TokenType_Operator,

    C_TokenType_Paren,
    C_TokenType_Brace,
    C_TokenType_Bracket,
    C_TokenType_Semicolon,
    C_TokenType_Comma,
    C_TokenType_Equals,

    C_TokenType_Comment,

    C_TokenType_Macro,

    // NOTE(nick): these are really more parser types
    C_ParserType_Keyword,
    C_ParserType_Token,
    C_ParserType_Function,

    C_TokenType_COUNT,
};

String __c_token_type_lt[] = {
    S("Unknown"),
    S("Number"),
    S("String"),
    S("Literal"),
    S("Identifier"),
    S("Operator"),
    S("Paren"),
    S("Brace"),
    S("Bracket"),
    S("Semicolon"),
    S("Comma"),
    S("Equals"),
    S("Comment"),
    S("Macro"),
    S("Keyword"),
    S("Type"),
    S("Function"),
    S("Constant"),
    S("COUNT"),
};

struct C_Token {
    C_Token_Type type;
    String     value;
};

struct C_Token_Array {
    Arena *arena;
    C_Token *data;
    i64 count;
    i64 capacity;
};

C_Token_Array c_tokenize(Arena *arena, String text)
{
    C_Token_Array tokens = {};
    array_init_from_arena(&tokens, arena, 2048);

    i64 i = 0;
    while (i < text.count)
    {
        char it = text[i];

        // Whitespace
        if (char_is_whitespace(it))
        {
            i64 start = i;
            while (i < text.count && char_is_whitespace(text[i])) {
                i += 1;
            }

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
                while (i < text.count && text.data[i] != '\n')
                {
                    i += 1;
                }

                auto token = array_push(&tokens, {0});
                token->type = C_TokenType_Comment;
                token->value = string_slice(text, start, i);
                i += 1;
                continue;
            }

            if (text.data[i + 1] == '*')
            {
                i64 start = i;
                auto at = string_slice(text, i + 2, text.count);
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

                auto token = array_push(&tokens, {0});
                token->type = C_TokenType_Comment;
                token->value = value;
                i += 2;
                continue;
            }
        }

        // Strings
        if (it == '"' || it == '\'')
        {
            char closing_char = it;
            i64 start = i;
            bool escape = false;

            i += 1;
            while (i < text.count)
            {
                if (text[i] == closing_char && !escape)
                {
                    break;
                }

                if (escape && text[i] == '\\')
                {
                    escape = false;
                }
                else
                {
                    escape = text[i] == '\\';
                }

                i ++;
            }
            i += 1;

            auto token = array_push(&tokens, {0});
            token->type = C_TokenType_String;
            token->value = string_slice(text, start, i);
            continue;
        }

        // Macros
        if (it == '#')
        {
            i64 start = i;
            i += 1;
            while (i < text.count && char_is_alpha(text[i])) i += 1;

            auto token = array_push(&tokens, {0});
            token->type = C_TokenType_Macro;
            token->value = string_slice(text, start, i);
            continue;
        }

        // Numbers
        if ((it == '.' && char_is_digit(text[i + 1])) || char_is_digit(it))
        {
            i64 start = i;
            i ++;

            // prefixes
            if (it == '0' && (char_to_lower(text[i]) == 'x' || char_to_lower(text[i]) == 'b'))
            {
                i ++;
            }

            while (
                i < text.count && (
                char_is_digit(text[i]) ||
                (text[i] == '.' || text[i] == '_' || text[i] == '-' || text[i] == '+' || text[i] == 'e') ||
                ('a' <= char_to_lower(text[i]) && char_to_lower(text[i]) >= 'f')
                )
            ) {
                i ++;
            }

            // @Incomplete: suffixes

            auto token = array_push(&tokens, {0});
            token->type = C_TokenType_Number;
            token->value = string_slice(text, start, i);
            continue;
        }

        // Literals
        // true, false, null, nullptr, NULL
        if (char_is_alpha(it))
        {
            static String literals[] = {S("true"), S("false"), S("NULL"), S("null"), S("nullptr")};

            String slice = string_skip(text, i);
            bool match = false;
            for (int j = 0; j < count_of(literals); j++)
            {
                auto it = literals[j];
                if (string_starts_with(slice, it))
                {
                    auto token = array_push(&tokens, {0});
                    token->type = C_TokenType_Literal;
                    token->value = string_slice(text, i, i + it.count);
                    i += it.count;
                    match = true;
                    break;
                }
            }

            if (match) continue;
        }

        // Identifiers
        if (char_is_alpha(it) || it == '$' || it == '_')
        {
            i64 start = i;
            while (i < text.count &&
                (char_is_alpha(text[i]) || char_is_digit(text[i]) || text[i] == '_' || text[i] == '$')
            )
            {
                i ++;
            }

            auto token = array_push(&tokens, {0});
            token->type = C_TokenType_Identifier;
            token->value = string_slice(text, start, i);
            continue;
        }

        // Operators
        // @Incomplete: distinguish between bit-wise ops, math ops, assignment and compare, etc etc

        if (it == ';')
        {
            auto token = array_push(&tokens, {0});
            token->type = C_TokenType_Semicolon;
            token->value = string_slice(text, i, i + 1);
            i += 1;
            continue;
        }

        if (it == '(' || it == ')')
        {
            auto token = array_push(&tokens, {0});
            token->type = C_TokenType_Paren;
            token->value = string_slice(text, i, i + 1);
            i += 1;
            continue;
        }

        if (
            it == '+' ||
            it == '-' ||
            it == '=' ||
            it == '*' ||
            it == '/' ||
            it == '>' ||
            it == '<' ||

            it == '!' ||

            it == '|' ||
            it == '~' ||
            it == '^' ||
            it == '&' ||

            it == '(' ||
            it == ')' ||
            it == '{' ||
            it == '}' ||
            it == '[' ||
            it == ']' ||
            it == ',' ||
            false
        )
        {
            auto token = array_push(&tokens, {0});
            token->type = C_TokenType_Operator;
            token->value = string_slice(text, i, i + 1);
            i += 1;
            continue;
        }

        auto token = array_push(&tokens, {0});
        token->type = C_TokenType_Unknown;
        token->value = string_slice(text, i, i + 1);
        i ++;
    }

    return tokens;
}

String c_token_type_to_string(C_Token_Type type)
{
    if (type >= 0 && type < C_TokenType_COUNT)
    {
        return __c_token_type_lt[type];
    }
    return S("");
}

void print_tokens(C_Token_Array tokens)
{
    For (tokens) {
        auto type = c_token_type_to_string(it.type);
        print("Token { type=%S, text=\"%S\" }\n", type, it.value);
    }
}

i64 c_token_loc(C_Token *it, String code)
{
    return it->value.data - code.data;
}

String c_whitespace_before_token(C_Token *it, String code)
{
    i64 loc = c_token_loc(it, code);
    if (loc > 0 && char_is_whitespace(code[loc - 1]))
    {
        loc -= 1;
        i64 start = loc;
        while (loc > 0 && char_is_whitespace(code[loc])) {
            loc -= 1;
        }
        return string_slice(code, loc + 1, start + 1);
    }
    return S("");
}

void c_convert_token_c_like(C_Token *it, C_Token *prev)
{
    if (it->type == C_TokenType_Identifier)
    {
        auto lower = string_lower(temp_arena(), it->value);
        if (
            string_equals(lower, S("return")) ||
            string_equals(lower, S("if")) ||
            string_equals(lower, S("else")) ||
            string_equals(lower, S("static")) ||
            string_equals(lower, S("do")) ||
            string_equals(lower, S("while")) ||
            string_equals(lower, S("for")) ||
            string_equals(lower, S("continue")) ||
            string_equals(lower, S("switch")) ||
            string_equals(lower, S("case")) ||
            string_equals(lower, S("break")) ||
            string_equals(lower, S("default")) ||
            string_equals(lower, S("goto")) ||

            string_equals(lower, S("const")) ||
            string_equals(lower, S("constexpr")) ||
            string_equals(lower, S("mutable")) ||
            string_equals(lower, S("volatile")) ||

            string_equals(lower, S("sizeof")) ||
            string_equals(lower, S("namespace")) ||

            string_equals(lower, S("cast")) ||
            string_equals(lower, S("count_of")) ||
            string_equals(lower, S("size_of")) ||
            string_equals(lower, S("offset_of")) ||
            string_equals(lower, S("align_of")) ||

            string_equals(lower, S("global")) ||
            string_equals(lower, S("static")) ||
            string_equals(lower, S("internal")) ||
            string_equals(lower, S("local")) ||
            string_equals(lower, S("inline")) ||
            string_equals(lower, S("restrict")) ||
            string_equals(lower, S("import")) ||
            string_equals(lower, S("export")) ||
            string_equals(lower, S("extern")) ||

            string_ends_with(lower, S("_global")) ||
            string_ends_with(lower, S("_static")) ||
            string_ends_with(lower, S("_internal")) ||
            string_ends_with(lower, S("_local")) ||
            string_ends_with(lower, S("_inline")) ||
            string_ends_with(lower, S("_restrict")) ||
            string_ends_with(lower, S("_export")) ||
            string_ends_with(lower, S("_extern")) ||

            string_equals(lower, S("public")) ||
            string_equals(lower, S("protected")) ||
            string_equals(lower, S("private")) ||
            string_equals(lower, S("new")) ||
            string_equals(lower, S("delete")) ||

            string_equals(lower, S("__attribute__")) ||
            false
        )
        {
             it->type = C_ParserType_Keyword;
        }

        if (
            string_equals(lower, S("struct")) ||
            string_equals(lower, S("union")) ||
            string_equals(lower, S("enum")) ||
            string_equals(lower, S("typedef")) ||
            string_equals(lower, S("class")) ||

            string_equals(lower, S("auto")) ||

            string_equals(lower, S("int")) ||
            string_equals(lower, S("void")) ||
            string_equals(lower, S("char")) ||
            string_equals(lower, S("bool")) ||
            string_equals(lower, S("float")) ||
            string_equals(lower, S("double")) ||
            string_equals(lower, S("signed")) ||
            string_equals(lower, S("unsigned")) ||
            string_ends_with(lower, S("_t")) ||

            string_equals(lower, S("u8")) ||
            string_equals(lower, S("i8")) ||
            string_equals(lower, S("u16")) ||
            string_equals(lower, S("i16")) ||
            string_equals(lower, S("u32")) ||
            string_equals(lower, S("i32")) ||
            string_equals(lower, S("u64")) ||
            string_equals(lower, S("i64")) ||
            string_equals(lower, S("f16")) ||
            string_equals(lower, S("f32")) ||
            string_equals(lower, S("f64")) ||
            string_equals(lower, S("f128")) ||
            string_equals(lower, S("usize")) ||
            string_equals(lower, S("isize")) ||
            string_equals(lower, S("b8")) ||
            string_equals(lower, S("b16")) ||
            string_equals(lower, S("b32")) ||
            false
        )
        {
            it->type = C_ParserType_Token;
        }
    }

    // NOTE(nick): some really basic parsing
    if (prev)
    {
        if (it->type == C_TokenType_Identifier && prev->type == C_TokenType_Identifier)
        {
            prev->type = C_ParserType_Token;
        }

        if (it->type == C_TokenType_Paren && it->value.data[0] == '(' && prev->type == C_TokenType_Identifier)
        {
            prev->type = C_ParserType_Function;
        }
    }
}

void c_convert_tokens_to_c_like(C_Token_Array tokens)
{
    For_Index (tokens) {
        auto it = &tokens.data[index];
        auto prev = index > 0 ? &tokens.data[index - 1] : NULL;
        c_convert_token_c_like(it, prev);
    }
}