#ifndef DCC_AST_NODE_HH
#define DCC_AST_NODE_HH

#include <memory>
#include <util/source_manager.hh>

namespace dcc::ast
{
    class Visitor;

    class Node
    {
    public:
        explicit constexpr Node(sm::SourceRange range) noexcept : m_range{range} {}
        virtual ~Node() = default;

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        [[nodiscard]] constexpr sm::SourceRange range() const noexcept { return m_range; }
        [[nodiscard]] constexpr sm::Location begin_loc() const noexcept { return m_range.begin; }
        [[nodiscard]] constexpr sm::Location end_loc() const noexcept { return m_range.end; }

        virtual void accept(Visitor& visitor) const = 0;

    private:
        sm::SourceRange m_range;
    };

    using NodePtr = std::unique_ptr<Node>;

    class Expr : public Node
    {
    public:
        using Node::Node;
    };

    class TypeExpr : public Node
    {
    public:
        using Node::Node;
    };

    class Stmt : public Node
    {
    public:
        using Node::Node;
    };

    class Decl : public Node
    {
    public:
        using Node::Node;
    };

    class Pattern : public Node
    {
    public:
        using Node::Node;
    };

} // namespace dcc::ast

#endif /* DCC_AST_NODE_HH */
