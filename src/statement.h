#pragma once

#include "runtime.h"

#include <functional>

namespace ast {

// ---------- ValueStatement ----------

using Statement = runtime::Executable;

// Выражение, возвращающее значение типа T,
// используется как основа для создания констант
template <typename T>
class ValueStatement : public Statement {
public:
    explicit ValueStatement(T v) : value_(std::move(v)) {}

    runtime::ObjectHolder Execute(runtime::Closure& /*closure*/,
                                  runtime::Context& /*context*/) override {
        return runtime::ObjectHolder::Share(value_);
    }

private:
    T value_;
};

// ---------- VariableValue -----------

using NumericConst = ValueStatement<runtime::Number>;
using StringConst = ValueStatement<runtime::String>;
using BoolConst = ValueStatement<runtime::Bool>;

/*
Вычисляет значение переменной либо цепочки вызовов полей объектов id1.id2.id3.
Например, выражение circle.center.x - цепочка вызовов полей объектов в инструкции:
x = circle.center.x
*/
class VariableValue : public Statement {
public:
    explicit VariableValue(const std::string& var_name)
        : dotted_ids_{std::move(var_name)} {}

    explicit VariableValue(std::vector<std::string> dotted_ids)
        : dotted_ids_(std::move(dotted_ids)) {}

    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;

private:
    std::vector<std::string> dotted_ids_;
};

// ---------- Assignment --------------

// Присваивает переменной, имя которой задано в параметре var, значение выражения rv
class Assignment : public Statement {
public:
    Assignment(std::string var, std::unique_ptr<Statement> rv)
        : var_(std::move(var))
        , rv_(std::move(rv)) {}

    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override {
        return closure[var_] = rv_->Execute(closure, context);
    }

private:
    std::string var_;
    std::unique_ptr<Statement> rv_;
};

// ---------- FieldAssignment ---------

// Присваивает полю object.field_name значение выражения rv
class FieldAssignment : public Statement {
public:
    FieldAssignment(VariableValue object,
                    std::string field_name,
                    std::unique_ptr<Statement> rv)
        : object_(std::move(object))
        , field_name_(std::move(field_name))
        , rv_(std::move(rv)) {}

    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override {
        runtime::ClassInstance* instance_ptr = object_.Execute(closure, context)
            .TryAs<runtime::ClassInstance>();
        return instance_ptr
               ? instance_ptr->Fields()[field_name_] = rv_->Execute(closure, context)
               : runtime::ObjectHolder::None();
    }

private:
    VariableValue object_;
    std::string field_name_;
    std::unique_ptr<Statement> rv_;
};

// ---------- None --------------------

// Значение None
class None : public Statement {
public:
    inline runtime::ObjectHolder Execute(
        [[maybe_unused]] runtime::Closure& closure,
        [[maybe_unused]] runtime::Context& context
    ) override {
        return {};
    }
};

// ---------- Print -------------------

// Команда print
class Print : public Statement {
public:
    // Инициализирует команду print для вывода значения выражения argument
    explicit Print(std::unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    // Инициализирует команду print для вывода списка значений args
    explicit Print(std::vector<std::unique_ptr<Statement>> args) 
        : args_(std::move(args)) {}

    // Инициализирует команду print для вывода значения переменной name
    inline static std::unique_ptr<Print> Variable(const std::string& name) {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    // Во время выполнения команды print вывод должен осуществляться в поток, возвращаемый из
    // context.GetOutputStream()
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;

private:
    std::vector<std::unique_ptr<Statement>> args_;
};

// ---------- MethodCall --------------

// Вызывает метод object.method со списком параметров args
class MethodCall : public Statement {
public:
    MethodCall(std::unique_ptr<Statement> object,
               std::string method,
               std::vector<std::unique_ptr<Statement>> args)
        : object_(std::move(object))
        , method_(std::move(method))
        , args_(std::move(args)) {}

    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;

private:
    std::unique_ptr<Statement> object_;
    std::string method_;
    std::vector<std::unique_ptr<Statement>> args_;
};

// ---------- NewInstance -------------

/*
Создаёт новый экземпляр класса class_, передавая его конструктору набор параметров args.
Если в классе отсутствует метод __init__ с заданным количеством аргументов,
то экземпляр класса создаётся без вызова конструктора (поля объекта не будут проинициализированы):

class Person:
  def set_name(name):
    self.name = name

p = Person()
# Поле name будет иметь значение только после вызова метода set_name
p.set_name("Ivan")
*/
class NewInstance : public Statement {
public:
    explicit NewInstance(const runtime::Class& cls)
        : class_(cls) {}

    NewInstance(const runtime::Class& cls,
                std::vector<std::unique_ptr<Statement>> args)
        : class_(cls)
        , args_(std::move(args)) {}

    // Возвращает объект, содержащий значение типа ClassInstance
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;

private:
    runtime::ClassInstance class_;
    std::vector<std::unique_ptr<Statement>> args_;
};

// ---------- UnaryOperation ----------

// Базовый класс для унарных операций
class UnaryOperation : public Statement {
public:
    explicit UnaryOperation(std::unique_ptr<Statement> argument)
        : arg_(std::move(argument)) {}

protected:
    std::unique_ptr<Statement> arg_;
};

// ---------- Stringify ---------------

// Операция str, возвращающая строковое значение своего аргумента
class Stringify : public UnaryOperation {
public:
    using UnaryOperation::UnaryOperation;

    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;
};

// ---------- BinaryOperation ---------

// Родительский класс Бинарная операция с аргументами lhs и rhs
class BinaryOperation : public Statement {
public:
    BinaryOperation(std::unique_ptr<Statement> lhs, 
                    std::unique_ptr<Statement> rhs)
        : lhs_(std::move(lhs))
        , rhs_(std::move(rhs)) {}

protected:
    std::unique_ptr<Statement> lhs_, rhs_;
};

// ---------- Add ---------------------

// Возвращает результат операции + над аргументами lhs и rhs
class Add : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается сложение:
    //  число + число
    //  строка + строка
    //  объект1 + объект2, если у объект1 - пользовательский класс с методом __add__(rhs)
    // В противном случае при вычислении выбрасывается runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;
};

// ---------- Sub ---------------------

// Возвращает результат вычитания аргументов lhs и rhs
class Sub : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается вычитание:
    //  число - число
    // Если lhs и rhs - не числа, выбрасывается исключение runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;
};

// ---------- Mult --------------------

// Возвращает результат умножения аргументов lhs и rhs
class Mult : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается умножение:
    //  число * число
    // Если lhs и rhs - не числа, выбрасывается исключение runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;
};

// ---------- Div ---------------------

// Возвращает результат деления lhs и rhs
class Div : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Поддерживается деление:
    //  число / число
    // Если lhs и rhs - не числа, выбрасывается исключение runtime_error
    // Если rhs равен 0, выбрасывается исключение runtime_error
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;
};

// ---------- Or ----------------------

// Возвращает результат вычисления логической операции or над lhs и rhs
class Or : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Значение аргумента rhs вычисляется, только если значение lhs
    // после приведения к Bool равно False
    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override {
        return runtime::ObjectHolder::Own(runtime::Bool(
            !runtime::IsTrue(lhs_->Execute(closure, context))
            ? runtime::IsTrue(rhs_->Execute(closure, context))
            : true
        ));
    }
};

// ---------- And ---------------------

// Возвращает результат вычисления логической операции and над lhs и rhs
class And : public BinaryOperation {
public:
    using BinaryOperation::BinaryOperation;

    // Значение аргумента rhs вычисляется, только если значение lhs
    // после приведения к Bool равно True
    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override {
        return runtime::ObjectHolder::Own(runtime::Bool(
            runtime::IsTrue(lhs_->Execute(closure, context))
            ? runtime::IsTrue(rhs_->Execute(closure, context))
            : false
        ));
    }
};

// ---------- Not ---------------------

// Возвращает результат вычисления логической операции not над единственным аргументом операции
class Not : public UnaryOperation {
public:
    using UnaryOperation::UnaryOperation;

    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override {
        return runtime::ObjectHolder::Own(runtime::Bool(
            !runtime::IsTrue(arg_->Execute(closure, context))
        ));
    }
};

// ---------- Compound ----------------

// Составная инструкция (например: тело метода, содержимое ветки if, либо else)
class Compound : public Statement {
public:
    // Конструирует Compound из нескольких инструкций типа unique_ptr<Statement>
    template <typename... Args>
    explicit Compound(Args&&... args) {
        (statements_.push_back(std::forward<Args>(args)), ...);
    }

    // Добавляет очередную инструкцию в конец составной инструкции
    inline void AddStatement(std::unique_ptr<Statement> stmt) {
        statements_.push_back(std::move(stmt));
    }

    // Последовательно выполняет добавленные инструкции. Возвращает None
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;

private:
    std::vector<std::unique_ptr<Statement>> statements_;
};

// ---------- MethodBody --------------

// Тело метода. Как правило, содержит составную инструкцию
class MethodBody : public Statement {
public:
    explicit MethodBody(std::unique_ptr<Statement>&& body)
        : body_(std::move(body)) {}

    // Вычисляет инструкцию, переданную в качестве body.
    // Если внутри body была выполнена инструкция return, возвращает результат return
    // В противном случае возвращает None
    runtime::ObjectHolder Execute(runtime::Closure& closure,
                                  runtime::Context& context) override;

private:
    std::unique_ptr<Statement> body_;
};

// ---------- Return ------------------

// Выполняет инструкцию return с выражением statement
class Return : public Statement {
public:
    explicit Return(std::unique_ptr<Statement> statement)
        : statement_(std::move(statement)) {}

    // Останавливает выполнение текущего метода. После выполнения инструкции return метод,
    // внутри которого она была исполнена, должен вернуть результат вычисления выражения statement.
    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override {
        throw statement_->Execute(closure, context);
    }

private:
    std::unique_ptr<Statement> statement_;
};

// ---------- ClassDefinition ---------

// Объявляет класс
class ClassDefinition : public Statement {
public:
    // Гарантируется, что ObjectHolder содержит объект типа runtime::Class
    explicit ClassDefinition(runtime::ObjectHolder cls)
        : class_(std::move(cls)) {};

    // Создаёт внутри closure новый объект, совпадающий с именем класса и значением, переданным в
    // конструктор
    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context&) override {
        closure[class_.TryAs<runtime::Class>()->GetName()] = class_;
        return runtime::ObjectHolder::None();
    }

private:
    runtime::ObjectHolder class_;
};

// ---------- IfElse ------------------

// Инструкция if <condition> <if_body> else <else_body>
class IfElse : public Statement {
public:
    // Параметр else_body может быть равен nullptr
    IfElse(std::unique_ptr<Statement> condition,
           std::unique_ptr<Statement> if_body,
           std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body)) {}

    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override{
        if (runtime::IsTrue(condition_->Execute(closure, context)))
            return if_body_->Execute(closure, context);
        else if (else_body_)
            return else_body_->Execute(closure, context);
        else 
            return runtime::ObjectHolder::None();
    }

private:
    std::unique_ptr<Statement> condition_, if_body_, else_body_;
};

// ---------- Comparison --------------

// Операция сравнения
class Comparison : public BinaryOperation {
public:
    // Comparator задаёт функцию, выполняющую сравнение значений аргументов
    using Comparator = std::function<bool(const runtime::ObjectHolder&,
                                          const runtime::ObjectHolder&,
                                          runtime::Context&)>;

    Comparison(Comparator cmp,
               std::unique_ptr<Statement> lhs,
               std::unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(std::move(cmp)) {}

    // Вычисляет значение выражений lhs и rhs и возвращает результат работы comparator,
    // приведённый к типу runtime::Bool
    inline runtime::ObjectHolder Execute(runtime::Closure& closure,
                                         runtime::Context& context) override {
        return runtime::ObjectHolder::Own(runtime::Bool(cmp_(
            lhs_->Execute(closure, context),
            rhs_->Execute(closure, context),
            context
        )));
    }

private:
    Comparator cmp_;
};

}  // namespace ast