#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;

void AppendByStatement(std::stringstream& ss,
                       const std::unique_ptr<Statement>& statement,
                       Closure& closure,
                       Context& context) {
    if (ObjectHolder obj = statement->Execute(closure, context))
        obj->Print(ss, context);
    else
        ss << "None";
}

std::vector<ObjectHolder> ExecuteArguments(
    const std::vector<std::unique_ptr<Statement>>& args,
    Closure& closure,
    Context& context
) {
    std::vector<ObjectHolder> executed_args;
    executed_args.reserve(args.size());
    for (const std::unique_ptr<Statement>& argument : args)
        executed_args.push_back(argument->Execute(closure, context));
    return executed_args;
}
}  // namespace

// ---------- VariableValue -----------

ObjectHolder VariableValue::Execute(Closure& closure, Context&) {
    const auto& check_dotted_id = [](const Closure* closure,
                                     const std::string& dotted_id) {
        if (closure->find(dotted_id) == closure->end())
            throw std::runtime_error("variable " + dotted_id + " not found");
    };

    Closure* closure_ptr = &closure;
    for (auto it = dotted_ids_.begin(); it != dotted_ids_.end() - 1; ++it) {
        check_dotted_id(closure_ptr, *it);
        closure_ptr = &(*closure_ptr)[*it]
            .TryAs<runtime::ClassInstance>()->Fields();
    }

    check_dotted_id(closure_ptr, dotted_ids_.back());
    return (*closure_ptr)[dotted_ids_.back()];
}

// ---------- Print -------------------

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    std::stringstream ss;

    if (!args_.empty()) {
        AppendByStatement(ss, args_.front(), closure, context);
        std::for_each(
            std::next(args_.begin()),
            args_.end(),
            [&](const std::unique_ptr<Statement>& statement) {
                ss << ' ';
                AppendByStatement(ss, statement, closure, context);
            }
        );
    }
    ss << '\n';

    runtime::String(ss.str()).Print(context.GetOutputStream(), context);
    return ObjectHolder::None();
}

// ---------- MethodCall --------------

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    runtime::ClassInstance* instance_ptr = object_->Execute(closure, context)
        .TryAs<runtime::ClassInstance>();

    if (!(instance_ptr && instance_ptr->HasMethod(method_, args_.size())))
        throw std::runtime_error("not a class instance");

    return instance_ptr->Call(
        method_,
        ExecuteArguments(args_, closure, context),
        context
    );
}

// ---------- Stringify ---------------

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    std::stringstream ss;
    AppendByStatement(ss, arg_, closure, context);
    return ObjectHolder::Own(runtime::String(ss.str()));
}

// ---------- Add ---------------------

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_holder = lhs_->Execute(closure, context);
    ObjectHolder rhs_holder = rhs_->Execute(closure, context);

    {
        runtime::Number* lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        runtime::Number* rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        if (lhs_ptr && rhs_ptr) {
            return ObjectHolder::Own(runtime::Number(
                lhs_ptr->GetValue() + rhs_ptr->GetValue()
            ));
        }
    }
    {
        runtime::String* lhs_ptr = lhs_holder.TryAs<runtime::String>();
        runtime::String* rhs_ptr = rhs_holder.TryAs<runtime::String>();
        if (lhs_ptr && rhs_ptr) {
            return ObjectHolder::Own(runtime::String(
                lhs_ptr->GetValue() + rhs_ptr->GetValue()
            ));
        }
    }

    runtime::ClassInstance* lhs_instance_ptr = lhs_holder
        .TryAs<runtime::ClassInstance>();
    if (lhs_instance_ptr && lhs_instance_ptr->HasMethod(ADD_METHOD, 1u))
        return lhs_instance_ptr->Call(ADD_METHOD, {rhs_holder}, context);

    throw std::runtime_error("cannot add arguments");
}

// ---------- Sub ---------------------

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_holder = lhs_->Execute(closure, context);
    ObjectHolder rhs_holder = rhs_->Execute(closure, context);

    {
        runtime::Number* lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        runtime::Number* rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        if (lhs_ptr && rhs_ptr) {
            return ObjectHolder::Own(runtime::Number(
                lhs_ptr->GetValue() - rhs_ptr->GetValue()
            ));
        }
    }

    throw std::runtime_error(
        "cannot substract arguments (valid for numbers only)"
    );
}

// ---------- Mult --------------------

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_holder = lhs_->Execute(closure, context);
    ObjectHolder rhs_holder = rhs_->Execute(closure, context);
    {
        runtime::Number* lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        runtime::Number* rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        if (lhs_ptr && rhs_ptr) {
            return ObjectHolder::Own(runtime::Number(
                lhs_ptr->GetValue()*rhs_ptr->GetValue()
            ));
        }
    }

    throw std::runtime_error(
        "cannot multiply arguments (valid for numbers only)"
    );
}

// ---------- Div ---------------------

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_holder = lhs_->Execute(closure, context);
    ObjectHolder rhs_holder = rhs_->Execute(closure, context);
    {
        runtime::Number* lhs_ptr = lhs_holder.TryAs<runtime::Number>();
        runtime::Number* rhs_ptr = rhs_holder.TryAs<runtime::Number>();
        if (lhs_ptr && rhs_ptr) {
            if (rhs_ptr->GetValue() == 0)
                throw std::runtime_error("try to divide to zero");

            return ObjectHolder::Own(runtime::Number(
                lhs_ptr->GetValue()/rhs_ptr->GetValue()
            ));
        }
    }

    throw std::runtime_error(
        "cannot divide arguments (valid for numbers only)"
    );
}

// ---------- Compound ----------------

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (const auto& statement : statements_)
        statement->Execute(closure, context);
    return ObjectHolder::None();
}

// ---------- NewInstance -------------

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (class_.HasMethod(INIT_METHOD, args_.size()))
        class_.Call(
            INIT_METHOD,
            ExecuteArguments(args_, closure, context),
            context
        );
    return ObjectHolder::Share(class_);
}

// ---------- MethodBody --------------

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_->Execute(closure, context);
    } catch (const ObjectHolder& holder) {
        return holder;
    }
    return ObjectHolder::None();
}

}  // namespace ast