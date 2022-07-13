typedef u32 Token_Type;
enum {
    TokenType_NULL = 0,

    TokenType_Number,
    TokenType_String,
    TokenType_Identifier,
    TokenType_Symbol,

    TokenType_Whitespace,
    TokenType_Comment,

    TokenType_COUNT,
};

static String __token_type_name_lookup[] = {
    S("Null"),

    S("Number"),
    S("String"),
    S("Identifier"),
    S("Symbol"),

    S("Whitespace"),
    S("Comment"),

    S("COUNT"),
};

typedef u32 Token_Flag;
enum
{
    TokenFlag_Multiline_String,
    TokenFlag_OpenBracket,
    TokenFlag_CloseBracket,
};

struct Token
{
    Token_Type type;
    String value;
};

typedef u32 Node_Type;
enum {
    NodeType_NULL = 0,

    NodeType_Number,
    NodeType_String,
    NodeType_Identifier,
    NodeType_Tag,
    NodeType_Root,
    NodeType_Array,

    NodeType_COUNT,
};

static String __node_type_name_lookup[] = {
    S("Null"),

    S("Number"),
    S("String"),
    S("Identifier"),
    S("Tag"),
    S("Root"),
    S("Array"),

    S("COUNT"),
};

struct Node
{
    Node *next;
    Node *prev;
    Node *parent;

    Node *first_child;
    Node *last_child;

    Node *first_tag;
    Node *last_tag;

    Node_Type type;
    String    string;
    String    raw_string;
};

static Node __meta_nil_node = {};

struct Parser
{
    Array<Token> tokens;
    i64 index;

    Arena *arena;
};

Array<Token> tokenize(String text)
{
    Array<Token> tokens = {};
    array_init_from_allocator(&tokens, temp_allocator(), 256);

    i64 i = 0;
    while (i < text.count)
    {
        char it = text[i];

        // Whitespace
        if (char_is_whitespace(it))
        {
            i64 start = i;
            while (i < text.count && char_is_whitespace(text.data[i]))
            {
                i += 1;
            }

            /*
            auto token = array_push(&tokens);
            token->type = TokenType_Whitespace;
            token->value = string_slice(text, start, i);
            */
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
                    i += 1;
                }

                /*
                auto token = array_push(&tokens);
                token->type = TokenType_Comment;
                token->value = string_slice(text, start, i - 1); // ignore the newline
                */
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

                        string_advance(&at, 2);
                        i += 2;
                        continue;
                    }

                    if (string_starts_with(at, S("*/"))) {
                        scope_depth --;

                        string_advance(&at, 2);
                        i += 2;
                        continue;
                    }

                    string_advance(&at, 1);
                    i += 1;
                }

                auto value = string_slice(text, start, i + 2);

                /*
                auto token = array_push(&tokens);
                token->type = TokenType_Comment;
                token->value = value;
                */
                i += 2;
                continue;
            }
        }

        if (it == '"' || it == '\'' || it == '`')
        {
            // @Idea: support "triple" sequences (``` or """ or ''')
            bool is_triple = i < text.count - 2 && text.data[i + 1] == it && text.data[i + 2] == it;

            char closing_char = it;
            i64 start = i;
            bool escape = false;

            i += 1;
            if (is_triple) i += 2;

            while (i < text.count)
            {
                if (text.data[i] == closing_char && !escape)
                {
                    if (is_triple)
                    {
                        if (i < text.count - 2 && text.data[i + 1] == closing_char && text.data[i + 2] == closing_char)
                        {
                            i += 2;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                if (escape && text.data[i] == '\\')
                {
                    escape = false;
                }
                else
                {
                    escape = text.data[i] == '\\';
                }

                i ++;
            }
            i += 1;

            auto token = array_push(&tokens);
            token->type = TokenType_String;
            token->value = string_slice(text, start, i);
            continue;
        }

        bool has_sign = false;
        if (char_is_digit(it) || (has_sign = it == '-' || it == '+') || (it == '.'))
        {
            i64 start = i;
            i ++;

            while (i < text.count && (
                char_is_digit(text.data[i]) ||
                char_is_alpha(text.data[i]) ||
                text.data[i] == '.' ||
                text.data[i] == '-' ||
                text.data[i] == '+' ||
                text.data[i] == '.' ||
                text.data[i] == '_'
            ))
            {
                i ++;
            }

            auto token = array_push(&tokens);
            token->type = TokenType_Number;
            token->value = string_slice(text, start, i);
            continue;
        }

        if (char_is_alpha(it) || it == '_')
        {
            i64 start = i;
            while (i < text.count &&
                (char_is_alpha(text[i]) || char_is_digit(text[i]) || text[i] == '_' || text[i] == '$')
            )
            {
                i ++;
            }

            auto token = array_push(&tokens);
            token->type = TokenType_Identifier;
            token->value = string_slice(text, start, i);
            continue;
        }

        if (
            it == ':' || it == '=' || it == ',' || it == ';' ||
            it == '(' || it == '{' || it == '[' ||
            it == ')' || it == '}' || it == ']' ||
            it == '@'
        )
        {
            i64 symbol_length = 1;
            if (i < text.count - 1)
            {
                char next = text.data[i + 1] ;
                if (it == ':' && next == ':')
                {
                    symbol_length = 2;
                }
            }

            auto token = array_push(&tokens);
            token->type = TokenType_Symbol;
            token->value = string_slice(text, i, i + symbol_length);
            i += symbol_length;
            continue;
        }

        print("[tokenize] Unhandled token: '%c'\n", it);
        i ++;
    }

    return tokens;
}

String token_type_to_string(Token_Type type)
{
    String result = {};
    if (type >= TokenType_NULL && type < TokenType_COUNT)
    {
        result = __token_type_name_lookup[type];
    }
    return result;
}

String node_type_to_string(Node_Type type)
{
    String result = {};
    if (type >= NodeType_NULL && type < NodeType_COUNT)
    {
        result = __node_type_name_lookup[type];
    }
    return result;
}

void node_set_tags(Node *parent, Node *tags)
{
    if (tags)
    {
        parent->first_tag = tags;
    }
    else
    {
        parent->first_tag = &__meta_nil_node;
    }
}

void node_set_children(Node *parent, Node *children)
{
    if (children)
    {
        if (children->type == NodeType_Array)
        {
            children = children->first_child;
        }

        parent->first_child = children;
    }
    else
    {
        parent->first_child = &__meta_nil_node;
    }
}

bool node_is_nil(Node *node)
{
    return node->type == NodeType_NULL;
}

Node *make_node(Parser *state, Node_Type type, String str, Node *tags = NULL)
{
    Node *node = push_struct(state->arena, Node);
    *node = {};

    node->type = type;
    node->string = str;

    node->next = &__meta_nil_node;
    node->prev = &__meta_nil_node;
    node->first_child = &__meta_nil_node;
    node->last_child = &__meta_nil_node;
    node->first_tag = &__meta_nil_node;
    node->last_tag = &__meta_nil_node;

    node_set_tags(node, tags);
    return node;
}

void push_child_node(Node *parent, Node *child)
{
    if (node_is_nil(parent->first_child)) parent->first_child = child;

    if (node_is_nil(parent->last_child))
    {
        parent->last_child = child;
    }
    else
    {
        parent->last_child->next = child;
        parent->last_child = child;
    }
}

void push_children_nodes(Node *parent, Node *children)
{
    parent->first_child = children;

    if (children)
    {
        Node *last = children;
        while (!node_is_nil(last->next)) last = last->next;

        parent->last_child = last;
    }
}

void push_sibling_node(Node *node, Node *it)
{
    Node *at = node;
    while (!node_is_nil(at->next)) at = at->next;

    at->next = it;
}

Token *peek(Parser *state, i64 offset)
{
    if (state->index + offset < state->tokens.count)
    {
        return &state->tokens.data[state->index + offset];
    }

    return null;
}

void consume(Parser *state, Token *it)
{
    assert(&state->tokens[state->index] == it);
    state->index ++;
}

bool consume_any(Parser *state, Token_Type type)
{
    bool result = false;
    auto token = peek(state, 0);

    while (token && token->type == type)
    {
        result = true;
        consume(state, token);
        token = peek(state, 0);
    }

    return result;
}

// @Cleanup: make consume_any also look at token flags, make flags for separators, open and close parens
bool consume_any_separators(Parser *state)
{
    bool result = false;
    auto token = peek(state, 0);

    while (token && token->type == TokenType_Symbol && (token->value.data[0] == ',' || token->value.data[0] == ';'))
    {
        result = true;
        consume(state, token);
        token = peek(state, 0);
    }

    return result;
}

bool token_is_open_bracket(Token *it)
{
    char first = it->value.data[0];
    return it->type == TokenType_Symbol && (first == '{' || first == '[' || first == '(');
}

bool token_is_close_bracket(Token *it)
{
    char first = it->value.data[0];
    return it->type == TokenType_Symbol && (first == '}' || first == ']' || first == ')');
}

Node *parse_expression(Parser *state);
Node *parse_primary_expression(Parser *state);

Node *maybe_parse_tags(Parser *state)
{
    Node *result = null;
    auto it = peek(state, 0);

    while (it && it->type == TokenType_Symbol && it->value.data[0] == '@')
    {
        consume(state, it);

        auto next = peek(state, 0);
        if (next && next->type == TokenType_Identifier)
        {
            consume(state, next);
            auto tag_node = make_node(state, NodeType_Identifier, next->value);

            it = peek(state, 0);
            if (token_is_open_bracket(it))
            {
                auto tag_children = parse_expression(state);
                push_children_nodes(tag_node, tag_children);
            }

            if (!result) result = tag_node;
            else push_sibling_node(result, tag_node);
        }

        it = peek(state, 0);
    }

    return result;
}

Node *parse_primary_expression(Parser *state)
{
    auto tags = maybe_parse_tags(state);

    auto it = peek(state, 0);

    if (!it)
    {
        assert(!"Expected number, literal or string!");
        return null;
    }

    if (it->type == TokenType_Identifier)
    {
        consume(state, it);
        return make_node(state, NodeType_Identifier, it->value, tags);
    }

    if (it->type == TokenType_Number)
    {
        consume(state, it);
        return make_node(state, NodeType_Number, it->value, tags);
    }

    if (it->type == TokenType_String)
    {
        consume(state, it);
        return make_node(state, NodeType_String, it->value, tags);
    }

    if (token_is_open_bracket(it))
    {
        consume(state, it);

        Node *array = make_node(state, NodeType_Array, S("<array>"), NULL);

        auto token = peek(state, 0);
        while (token && !token_is_close_bracket(token))
        {
            consume_any_separators(state);
            auto next = parse_expression(state);
            push_child_node(array, next);
            consume_any_separators(state);

            token = peek(state, 0);
        }

        auto next = peek(state, 0);
        if (!next || !token_is_close_bracket(next))
        {
            print("Excpected closing paren, but got '%S' instead!\n", next->value);
            assert(!"Expected close paren!");
        }

        consume(state, next);

        consume_any_separators(state);

        node_set_tags(array, tags);
        return array;
    }

    dump(it->value);
    assert(!"Unexpected token!");

    return null;
}

Node *parse_expression(Parser *state)
{
    auto expr = parse_primary_expression(state);
    auto token = peek(state, 0);

    while (token && (token->type == TokenType_Symbol && (token->value.data[0] == '=' || token->value.data[0] == ':')))
    {
        consume(state, token);
        auto right = parse_primary_expression(state);
        node_set_children(expr, right);
        token = peek(state, 0);
    }

    return expr;
}

Node *parse(Arena *arena, Array<Token> tokens)
{
    Parser state = {};
    state.tokens = tokens;
    state.index = 0;
    state.arena = arena;

    Node *root = make_node(&state, NodeType_Root, S("<root>"), NULL);

    while (state.index < state.tokens.count)
    {
        Node *result = parse_expression(&state);
        if (result)
        {
            push_child_node(root, result);
        }
    }

    return root;
}

Node *parse_entire_string(Arena *arena, String text)
{
    auto tokens = tokenize(text);
    return parse(arena, tokens);
}

#define Each_Node(child, root) Node *child = root; !node_is_nil(child); child = child->next

bool node_has_tag(Node *it, String tag_name)
{
    bool result = false;

    for (Each_Node(tag, it->first_tag))
    {
        if (string_equals(tag->string, tag_name))
        {
            result = true;
            break;
        }
    }

    return result;
}

Node *find_child_by_name(Node *root, String name)
{
    Node *result = null;

    for (Each_Node(it, root->first_child))
    {
        if (string_equals(it->string, name))
        {
            result = it;
            break;
        }
    }
    return result;
}

Node *find_by_name(Node *start, String name)
{
    Node *result = null;

    for (Each_Node(it, start))
    {
        if (string_equals(it->string, name))
        {
            result = it;
            break;
        }
    }
    return result;
}

// @Incomplete: support triple quotes
String node_to_string(Node *it)
{
    String result = {};
    if (it && !node_is_nil(it))
    {
        result = string_slice(it->string, 1, it->string.count - 1);
    }
    return result;
}
