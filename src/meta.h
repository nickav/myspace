#ifndef META_H
#define META_H

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
    TokenFlag_Multiline    = 0x0001,
    TokenFlag_OpenBracket  = 0x0002,
    TokenFlag_CloseBracket = 0x0004,
    TokenFlag_Assign       = 0x0008,
};

struct Token
{
    Token_Type type;
    Token_Flag flags;
    String value;
};

typedef u32 Node_Type;
enum {
    NodeType_NULL = 0,

    NodeType_Number,
    NodeType_String,
    NodeType_Boolean,
    NodeType_Identifier,
    NodeType_Tag,
    NodeType_Array,
    NodeType_Root,
    NodeType_File,

    NodeType_COUNT,
};

static String __node_type_name_lookup[] = {
    S("Null"),

    S("Number"),
    S("String"),
    S("Boolean"),
    S("Identifier"),
    S("Tag"),
    S("Array"),
    S("Root"),
    S("File"),

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

static Node __meta_nil_node = {
    &__meta_nil_node,
    &__meta_nil_node,
    &__meta_nil_node,
    &__meta_nil_node,
    &__meta_nil_node,
    &__meta_nil_node,
    &__meta_nil_node,
};

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
            token->flags = (is_triple ? TokenFlag_Multiline : 0);
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
                char next = text.data[i + 1];
                if (it == ':' && next == ':')
                {
                    symbol_length = 2;
                }
            }

            Token_Flag flags = 0;
            if (it == ':')
            {
                flags |= TokenFlag_Assign;
            }
            else if (it == '[' || it == '{' || it == '(')
            {
                flags |= TokenFlag_OpenBracket;
            }
            else if (it == ']' || it == '}' || it == ')')
            {
                flags |= TokenFlag_CloseBracket;
            }

            auto token = array_push(&tokens);
            token->type = TokenType_Symbol;
            token->value = string_slice(text, i, i + symbol_length);
            flags = flags;

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
    return node == NULL || node->type == NodeType_NULL;
}

bool node_is_array(Node *node)
{
    return node && node->type == NodeType_Array;
}

bool node_is_identifier(Node *node)
{
    return node && node->type == NodeType_Array;
}

Node *nil_node() { return &__meta_nil_node; }

Node *make_node(Arena *arena, Node_Type type, String str, Node *tags = NULL)
{
    Node *node = push_struct(arena, Node);
    *node = {};

    // Set up nil children
    node->next = &__meta_nil_node;
    node->prev = &__meta_nil_node;
    node->first_child = &__meta_nil_node;
    node->last_child = &__meta_nil_node;
    node->first_tag = &__meta_nil_node;
    node->last_tag = &__meta_nil_node;


    node->type = type;
    node->string = str;

    if (type != NodeType_Array && type != NodeType_Root)
    {
        node->raw_string = str;
    }

    if (type == NodeType_String)
    {
        char quote_char = str.data[0];
        bool is_triple = str.count >= 3 && str.data[1] == quote_char && str.data[2] == quote_char;

        if (is_triple)
        {
            bool was_closed = str.data[str.count - 1] == quote_char && str.data[str.count - 2] == quote_char;
            node->string = string_slice(str, 3, str.count - (was_closed ? 3 : 0));
        }
        else
        {
            node->string = string_slice(str, 1, str.count - 1);
        }
    }

    node_set_tags(node, tags);
    return node;
}

Node *empty_node()
{
    return make_node(temp_arena(), NodeType_NULL, S(""), 0);
}

void node_push_child(Node *parent, Node *child)
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

void node_push_sibling(Node *node, Node *it)
{
    Node *at = node;
    while (!node_is_nil(at->next)) at = at->next;

    at->next = it;
}

Node *node_from_string(String str)
{
    return make_node(temp_arena(), NodeType_Identifier, string_temp(str));
}

void node_push_key_value(Node *node, String key, Node *value)
{
    auto child = node_from_string(key);
    node_push_child(node, child);
    node_push_child(child, value);
}

Node *make_array_node(Node *child1, Node *child2)
{
    Node *array = make_node(temp_arena(), NodeType_Array, S("<array>"), NULL);
    node_push_child(array, child1);
    node_push_child(array, child2);
    return array;
}

Node *make_array_node(Node *child1, Node *child2, Node *child3)
{
    Node *array = make_node(temp_arena(), NodeType_Array, S("<array>"), NULL);
    node_push_child(array, child1);
    node_push_child(array, child2);
    node_push_child(array, child3);
    return array;
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

bool token_has_flags(Token *it, Token_Flag flags)
{
    return (it->flags & flags) == flags;
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
            auto tag_node = make_node(state->arena, NodeType_Tag, next->value);

            it = peek(state, 0);
            if (it && token_is_open_bracket(it))
            {
                auto tag_children = parse_expression(state);
                push_children_nodes(tag_node, tag_children);
            }

            if (!result) result = tag_node;
            else node_push_sibling(result, tag_node);
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
        if (tags)
        {
            return tags;
        }
        
        assert(!"Expected number, literal or string!");
        return null;
    }

    if (it->type == TokenType_Identifier)
    {
        consume(state, it);
        return make_node(state->arena, NodeType_Identifier, it->value, tags);
    }

    if (it->type == TokenType_Number)
    {
        consume(state, it);
        return make_node(state->arena, NodeType_Number, it->value, tags);
    }

    if (it->type == TokenType_String)
    {
        consume(state, it);
        return make_node(state->arena, NodeType_String, it->value, tags);
    }

    if (it && token_is_open_bracket(it))
    {
        consume(state, it);

        Node *array = make_node(state->arena, NodeType_Array, S("<array>"), NULL);

        auto token = peek(state, 0);
        while (token && !token_is_close_bracket(token))
        {
            consume_any_separators(state);
            auto next = parse_expression(state);
            node_push_child(array, next);
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

    Node *root = make_node(state.arena, NodeType_Root, S("<root>"), NULL);

    while (state.index < state.tokens.count)
    {
        Node *result = parse_expression(&state);
        if (result)
        {
            node_push_child(root, result);
        }
    }

    return root;
}

Node *parse_entire_string(Arena *arena, String text)
{
    auto tokens = tokenize(text);
    auto root = parse(arena, tokens);
    root->raw_string = text;
    return root;
}

Node *parse_entire_file(Arena *arena, String path)
{
    auto text = os_read_entire_file(path);
    if (text.count)
    {
        auto result = parse_entire_string(arena, text);
        result->type = NodeType_File;
        result->string = path;
        return result;
    }
    return &__meta_nil_node;
}

#define Each_Node(child, root) Node *child = root; !node_is_nil(child); child = child->next

Node *node_find_child(Node *root, String name)
{
    Node *result = &__meta_nil_node;
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

Node *node_find_key_value(Node *root, String key)
{
    return node_find_child(root, key)->first_child;
}

Node *node_get_child(Node *root, i64 child_index)
{
    Node *result = nil_node();
    if (root)
    {
        Node *at = root->first_child;
        while (child_index > 0 && !node_is_nil(at))
        {
            at = at->next;
            child_index -= 1;
        }

        if (!node_is_nil(at)) result = at;
    }
    return result;
}

Node *node_find_tag(Node *it, String tag_name)
{
    Node *result = null;

    for (Each_Node(tag, it->first_tag))
    {
        if (string_equals(tag->string, tag_name))
        {
            result = tag;
            break;
        }
    }

    return result;
}

Node *node_get_tag(Node *root, i64 tag_index)
{
    Node *result = nil_node();

    if (root)
    {
        Node *at = root->first_tag;
        while (tag_index > 0 && !node_is_nil(at))
        {
            at = at->next;
            tag_index -= 1;
        }

        if (!node_is_nil(at)) result = at;
    }
    return result;
}

i64 node_count_children(Node *root)
{
    i64 result = 0;
    Node *node = root->first_child;
    while (!node_is_nil(node))
    {
        node = node->next;
        result += 1;
    }
    return result;
}

bool node_has_children(Node *node)
{
    return !node_is_nil(node->first_child);
}

Node *find_by_name(Node *start, String name)
{
    Node *result = &__meta_nil_node;

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

#if 0
void meta_test(String text)
{
    auto tokens = tokenize(text);

    print("--- Tokenize ---\n");

    For (tokens)
    {
        print("%S: %S\n", token_type_to_string(it.type), it.value);
    }

    auto root = parse(temp_arena(), tokens);

    print("\n");
    print("--- Parse ---\n");

    for (Each_Node(it, root->first_child))
    {
        print("Node: %S ", it->string);

        for (Each_Node(tag, it->first_tag))
        {
            print("@%S ", tag->string);
        }

        print("\n");

        for (Each_Node(child, it->first_child))
        {
            print("  Child: %S\n", child->string);
        }
    }

    auto title = find_by_name(root->first_child, S("title"));
    dump(title->first_child->string);
}
#endif

#endif // META_H
