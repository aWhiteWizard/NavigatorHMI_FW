/*
 * @FilePath: \NavigatorHMI_FW\src\runtime\expressionengine.cpp
 * @Description: 条件表达式引擎实现（I-3）——tokenizer + 递归下降 parser + evaluator
 *               语法见 expressionengine.h（首版最小集：数值比较/逻辑 + abs/round/min/max/now + value 关键字）
 */
#include "runtime/expressionengine.h"
#include "runtime/datamanager.h"
#include "runtime/projectmodel.h"

#include <QDebug>
#include <QChar>
#include <QDateTime>
#include <QElapsedTimer>
#include <QVariant>
#include <QtMath>
#include <cmath>

namespace navihmi {

// ═══ tokenizer ═══

namespace {

/// 告警节流（I-3 审查修复）：高频事件（onTimer/onValueChange）下防刷屏——同源告警 5s 窗口限 1 条
static bool warnThrottle()
{
    static QElapsedTimer t;
    static bool started = false;
    if (!started) { t.start(); started = true; return true; }
    if (t.elapsed() >= 5000) { t.restart(); return true; }
    return false;
}

enum class Tok {
    Number, Ident, String, Op, LParen, RParen, End, Error
};

struct Token {
    Tok type = Tok::End;
    double num = 0;
    QString text;      // Ident/String/Op 文本
    Token() = default;
    Token(Tok t, double n = 0, QString s = QString())
        : type(t), num(n), text(std::move(s)) {}
};

/// 操作符（多字符优先）
static const char* const TWO_CHAR_OPS[] = { ">=", "<=", "==", "!=", "&&", "||" };

class Lexer
{
public:
    explicit Lexer(const QString& src) : m_src(src) {}

    Token next()
    {
        skipSpace();
        if (m_pos >= m_src.size()) return Token{ Tok::End };
        const QChar c = m_src.at(m_pos);
        if (c.isDigit() || (c == QLatin1Char('.') && m_pos + 1 < m_src.size() && m_src.at(m_pos + 1).isDigit()))
            return lexNumber();
        if (c == QLatin1Char('"'))
            return lexString();
        if (c.isLetter() || c == QLatin1Char('_'))
            return lexIdent();
        if (c == QLatin1Char('(')) { ++m_pos; return Token{ Tok::LParen }; }
        if (c == QLatin1Char(')')) { ++m_pos; return Token{ Tok::RParen }; }
        // 操作符
        for (const char* op : TWO_CHAR_OPS) {
            const QString s = QString::fromLatin1(op);
            if (m_src.mid(m_pos, 2) == s) { m_pos += 2; return Token{ Tok::Op, 0, s }; }
        }
        if (c == QLatin1Char(',') || c == QLatin1Char('%')) {   // I-3 审查修复：函数参数分隔逗号 + 取模
            ++m_pos;
            return Token{ Tok::Op, 0, QString(c) };
        }
        if (c == QLatin1Char('!') || c == QLatin1Char('*') || c == QLatin1Char('/')
            || c == QLatin1Char('+') || c == QLatin1Char('-') || c == QLatin1Char('>')
            || c == QLatin1Char('<') || c == QLatin1Char('=')) {
            ++m_pos;
            return Token{ Tok::Op, 0, QString(c) };
        }
        return Token{ Tok::Error };
    }

private:
    void skipSpace() { while (m_pos < m_src.size() && m_src.at(m_pos).isSpace()) ++m_pos; }

    Token lexNumber()
    {
        int start = m_pos;
        while (m_pos < m_src.size() && (m_src.at(m_pos).isDigit() || m_src.at(m_pos) == QLatin1Char('.'))) ++m_pos;
        bool ok = false;
        const double v = m_src.mid(start, m_pos - start).toDouble(&ok);
        return Token{ ok ? Tok::Number : Tok::Error, v };
    }

    Token lexString()
    {
        ++m_pos;   // 开引号
        int start = m_pos;
        while (m_pos < m_src.size() && m_src.at(m_pos) != QLatin1Char('"')) ++m_pos;
        // I-3 审查修复：未闭合字符串（扫到结尾未见闭引号）→ Error（防畸形条件静默接受）
        if (m_pos >= m_src.size()) return Token{ Tok::Error };
        const QString s = m_src.mid(start, m_pos - start);
        ++m_pos;   // 闭引号
        return Token{ Tok::String, 0, s };
    }

    Token lexIdent()
    {
        int start = m_pos;
        while (m_pos < m_src.size() && (m_src.at(m_pos).isLetterOrNumber() || m_src.at(m_pos) == QLatin1Char('_'))) ++m_pos;
        return Token{ Tok::Ident, 0, m_src.mid(start, m_pos - start) };
    }

    const QString& m_src;
    int m_pos = 0;
};

// ═══ AST + evaluator ═══

struct Value {
    double num = 0;
    QString str;
    bool isString = false;
    Value() = default;
    explicit Value(double n) : num(n) {}
    Value(double n, QString s, bool strFlag) : num(n), str(std::move(s)), isString(strFlag) {}
};

struct Node {
    virtual ~Node() = default;
    virtual Value eval(const DataManager* dm, const Widget* widget) const = 0;
};

struct NumNode : Node {
    explicit NumNode(Value v) : val(std::move(v)) {}
    Value eval(const DataManager*, const Widget*) const override { return val; }
    Value val;
};

struct VarNode : Node {
    QString name;
    bool isValueKeyword = false;
    Value eval(const DataManager* dm, const Widget* widget) const override
    {
        QString tag = name;
        if (isValueKeyword) {
            tag = widget ? widget->boundTag : QString();
            if (tag.isEmpty()) {
                if (warnThrottle()) qWarning().noquote() << "[COND] value 关键字无绑定变量，按 0 处理";
                return Value{ 0 };
            }
        }
        if (!dm || !dm->hasTag(tag)) {
            if (warnThrottle()) qWarning().noquote() << "[COND] 变量不存在:" << tag << "按 0 处理";
            return Value{ 0 };
        }
        const QVariant v = dm->value(tag);
        if (v.typeId() == QMetaType::QString || v.typeId() == QMetaType::QByteArray) {
            Value r; r.isString = true; r.str = v.toString();
            bool ok = false; r.num = r.str.toDouble(&ok);   // 数值字符串兼作 num
            return r;
        }
        return Value{ v.toDouble() };
    }
};

struct UnaryNode : Node {
    bool negate = false;   // 一元负号
    bool not_ = false;     // 逻辑非
    std::unique_ptr<Node> child;
    Value eval(const DataManager* dm, const Widget* widget) const override
    {
        const Value v = child->eval(dm, widget);
        if (not_) return Value{ v.num == 0 ? 1.0 : 0.0 };
        if (negate) return Value{ -v.num };
        return v;
    }
};

struct BinNode : Node {
    QString op;
    std::unique_ptr<Node> left, right;
    Value eval(const DataManager* dm, const Widget* widget) const override
    {
        if (op == QLatin1String("&&")) {
            const Value l = left->eval(dm, widget);
            return Value{ (l.num != 0 && right->eval(dm, widget).num != 0) ? 1.0 : 0.0 };   // 短路
        }
        if (op == QLatin1String("||")) {
            const Value l = left->eval(dm, widget);
            return Value{ (l.num != 0 || right->eval(dm, widget).num != 0) ? 1.0 : 0.0 };
        }
        const Value l = left->eval(dm, widget);
        const Value r = right->eval(dm, widget);
        if (op == QLatin1String("+")) return Value{ l.num + r.num };
        if (op == QLatin1String("-")) return Value{ l.num - r.num };
        if (op == QLatin1String("*")) return Value{ l.num * r.num };
        if (op == QLatin1String("/")) return Value{ r.num == 0 ? 0.0 : l.num / r.num };
        if (op == QLatin1String("%")) return Value{ r.num == 0 ? 0.0 : std::fmod(l.num, r.num) };
        // 比较
        if (op == QLatin1String("==")) return Value{ valueEquals(l, r) ? 1.0 : 0.0 };
        if (op == QLatin1String("!=")) return Value{ valueEquals(l, r) ? 0.0 : 1.0 };
        if (l.isString || r.isString) {
            if (warnThrottle()) qWarning().noquote() << "[COND] 字符串仅支持 ==/!= 比较，按 false 处理";
            return Value{ 0 };
        }
        if (op == QLatin1String(">"))  return Value{ l.num >  r.num ? 1.0 : 0.0 };
        if (op == QLatin1String(">=")) return Value{ l.num >= r.num ? 1.0 : 0.0 };
        if (op == QLatin1String("<"))  return Value{ l.num <  r.num ? 1.0 : 0.0 };
        if (op == QLatin1String("<=")) return Value{ l.num <= r.num ? 1.0 : 0.0 };
        return Value{ 0 };
    }
    static bool valueEquals(const Value& a, const Value& b)
    {
        if (a.isString || b.isString) return a.str == b.str;
        return qFuzzyCompare(a.num, b.num);
    }
};

struct CallNode : Node {
    QString fn;
    std::vector<std::unique_ptr<Node>> args;
    Value eval(const DataManager* dm, const Widget* widget) const override
    {
        if (fn == QLatin1String("now")) return Value{ double(QDateTime::currentSecsSinceEpoch()) };
        if (args.empty()) {
            if (warnThrottle()) qWarning().noquote() << "[COND] 函数" << fn << "需要参数";
            return Value{ 0 };
        }
        const Value a = args[0]->eval(dm, widget);
        if (fn == QLatin1String("abs")) return Value{ qAbs(a.num) };
        if (fn == QLatin1String("round")) return Value{ double(qRound(a.num)) };
        if (fn == QLatin1String("min")) {
            if (args.size() < 2) { if (warnThrottle()) qWarning().noquote() << "[COND] min 需要 2 参数"; return Value{ 0 }; }
            const Value b = args[1]->eval(dm, widget);
            return Value{ qMin(a.num, b.num) };
        }
        if (fn == QLatin1String("max")) {
            if (args.size() < 2) { if (warnThrottle()) qWarning().noquote() << "[COND] max 需要 2 参数"; return Value{ 0 }; }
            const Value b = args[1]->eval(dm, widget);
            return Value{ qMax(a.num, b.num) };
        }
        if (warnThrottle()) qWarning().noquote() << "[COND] 未知函数:" << fn;
        return Value{ 0 };
    }
};

class Parser
{
public:
    Parser(Lexer& lex) : m_lex(lex) { m_cur = m_lex.next(); }

    /// 解析整表达式；成功返回节点（空表达式 → nullptr），失败返回 nullptr
    std::unique_ptr<Node> parse()
    {
        if (m_cur.type == Tok::End) return nullptr;
        auto node = parseOr();
        if (!node || m_cur.type != Tok::End) return nullptr;   // 尾随 token
        return node;
    }

private:
    std::unique_ptr<Node> parseOr()
    {
        auto left = parseAnd();
        while (left && m_cur.type == Tok::Op && m_cur.text == QLatin1String("||")) {
            m_cur = m_lex.next();
            auto right = parseAnd();
            if (!right) return nullptr;
            auto n = std::make_unique<BinNode>();
            n->op = QLatin1String("||"); n->left = std::move(left); n->right = std::move(right);
            left = std::move(n);
        }
        return left;
    }

    std::unique_ptr<Node> parseAnd()
    {
        // ! 优先级高于比较：parseAnd 直接进 parseCompare（parseUnary 内处理 ! 前缀）
        auto left = parseCompare();
        while (left && m_cur.type == Tok::Op && m_cur.text == QLatin1String("&&")) {
            m_cur = m_lex.next();
            auto right = parseCompare();
            if (!right) return nullptr;
            auto n = std::make_unique<BinNode>();
            n->op = QLatin1String("&&"); n->left = std::move(left); n->right = std::move(right);
            left = std::move(n);
        }
        return left;
    }

    std::unique_ptr<Node> parseCompare()
    {
        auto left = parseAdd();
        if (!left) return nullptr;
        if (m_cur.type == Tok::Op && (m_cur.text == QLatin1String(">") || m_cur.text == QLatin1String(">=")
            || m_cur.text == QLatin1String("<") || m_cur.text == QLatin1String("<=")
            || m_cur.text == QLatin1String("==") || m_cur.text == QLatin1String("!="))) {
            const QString op = m_cur.text;
            m_cur = m_lex.next();
            auto right = parseAdd();
            if (!right) return nullptr;
            auto n = std::make_unique<BinNode>();
            n->op = op; n->left = std::move(left); n->right = std::move(right);
            return n;
        }
        return left;
    }

    std::unique_ptr<Node> parseAdd()
    {
        auto left = parseMul();
        while (left && m_cur.type == Tok::Op && (m_cur.text == QLatin1String("+") || m_cur.text == QLatin1String("-"))) {
            const QString op = m_cur.text;
            m_cur = m_lex.next();
            auto right = parseMul();
            if (!right) return nullptr;
            auto n = std::make_unique<BinNode>();
            n->op = op; n->left = std::move(left); n->right = std::move(right);
            left = std::move(n);
        }
        return left;
    }

    std::unique_ptr<Node> parseMul()
    {
        auto left = parseUnary();
        while (left && m_cur.type == Tok::Op
               && (m_cur.text == QLatin1String("*") || m_cur.text == QLatin1String("/") || m_cur.text == QLatin1String("%"))) {
            const QString op = m_cur.text;
            m_cur = m_lex.next();
            auto right = parseUnary();
            if (!right) return nullptr;
            auto n = std::make_unique<BinNode>();
            n->op = op; n->left = std::move(left); n->right = std::move(right);
            left = std::move(n);
        }
        return left;
    }

    std::unique_ptr<Node> parseUnary()
    {
        // ! 最高优先级（附录 A）：!a == b → (!a) == b（! 吃一元单元后回落比较层）
        if (m_cur.type == Tok::Op && m_cur.text == QLatin1String("!")) {
            m_cur = m_lex.next();
            auto child = parseUnary();
            if (!child) return nullptr;
            auto n = std::make_unique<UnaryNode>();
            n->not_ = true; n->child = std::move(child);
            return n;
        }
        if (m_cur.type == Tok::Op && m_cur.text == QLatin1String("-")) {
            m_cur = m_lex.next();
            auto child = parseUnary();
            if (!child) return nullptr;
            auto n = std::make_unique<UnaryNode>();
            n->negate = true; n->child = std::move(child);
            return n;
        }
        return parsePrimary();
    }

    std::unique_ptr<Node> parsePrimary()
    {
        if (m_cur.type == Tok::Number) {
            auto n = std::make_unique<NumNode>(Value{ m_cur.num });
            m_cur = m_lex.next();
            return n;
        }
        if (m_cur.type == Tok::String) {
            Value v; v.isString = true; v.str = m_cur.text;
            bool ok = false; v.num = v.str.toDouble(&ok);
            auto n = std::make_unique<NumNode>(std::move(v));
            m_cur = m_lex.next();
            return n;
        }
        if (m_cur.type == Tok::LParen) {
            m_cur = m_lex.next();
            auto inner = parseOr();
            if (!inner || m_cur.type != Tok::RParen) return nullptr;
            m_cur = m_lex.next();
            return inner;
        }
        if (m_cur.type == Tok::Ident) {
            const QString name = m_cur.text;
            m_cur = m_lex.next();
            if (m_cur.type == Tok::LParen) {   // 函数调用
                m_cur = m_lex.next();
                auto n = std::make_unique<CallNode>();
                n->fn = name;
                if (m_cur.type != Tok::RParen) {
                    while (true) {
                        auto arg = parseOr();
                        if (!arg) return nullptr;
                        n->args.push_back(std::move(arg));
                        if (m_cur.type == Tok::RParen) break;
                        if (m_cur.type != Tok::Op || m_cur.text != QLatin1String(",")) return nullptr;
                        m_cur = m_lex.next();
                    }
                }
                m_cur = m_lex.next();   // 闭括号
                return n;
            }
            auto n = std::make_unique<VarNode>();
            n->name = name;
            n->isValueKeyword = (name == QLatin1String("value"));
            if (name == QLatin1String("true")) return std::make_unique<NumNode>(Value{ 1 });
            if (name == QLatin1String("false")) return std::make_unique<NumNode>(Value{ 0 });
            return n;
        }
        return nullptr;
    }

    Lexer& m_lex;
    Token m_cur;
};

} // namespace

bool ExprEngine::eval(const QString& expression, const DataManager* dm, const Widget* widget)
{
    const QString expr = expression.trimmed();
    if (expr.isEmpty()) return true;
    // I-3 审查修复：长度上限（防恶意/误粘贴长表达式栈溢出——条件来自可导入的 .navihmi，属不可信输入）
    if (expr.size() > 512) {
        if (warnThrottle()) qWarning().noquote() << "[COND] 表达式超长(>512 字符)，按 false 处理";
        return false;
    }
    Lexer lex(expr);
    Parser parser(lex);
    std::unique_ptr<Node> root = parser.parse();
    if (!root) {
        if (warnThrottle()) qWarning().noquote() << "[COND] 表达式解析失败:" << expr;
        return false;
    }
    const Value v = root->eval(dm, widget);
    return v.num != 0;
}

} // namespace navihmi
