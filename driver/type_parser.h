#pragma once

#include <list>
#include <stack>
#include <string>

struct TypeAst {
    enum Meta {
        Array,
        Null,
        Nullable,
        Number,
        Terminal,
        Tuple,
    };

    /// Type's category.
    Meta meta;
    /// Type's name.
    std::string name;
    /// Size of type's instance.  For fixed-width types only.
    size_t size = 0;
    /// Subelements of the type.
    std::list<TypeAst> elements;
};


class TypeParser {

    struct Token {
        enum Type {
            Invalid = 0,
            Name,
            Number,
            LPar,
            RPar,
            Comma,
            EOS,
        };

        Type type;
        std::string value;
    };

public:
    explicit TypeParser(const std::string& name);
    ~TypeParser();

    bool parse(TypeAst* type);

private:
    Token nextToken();

private:
    const char* cur_;
    const char* end_;

    TypeAst* type_;
    std::stack<TypeAst*> open_elements_;
};
