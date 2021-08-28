#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

// ---------- ObjectHolder ------------

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto*){}));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

// ---------- ValueObject -------------

bool IsTrue(const ObjectHolder& object) {
    if (const Number* ptr = object.TryAs<Number>())
        return ptr->GetValue() != 0;
    else if (const String* ptr = object.TryAs<String>())
        return !ptr->GetValue().empty();
    else if (const Bool* ptr = object.TryAs<Bool>())
        return ptr->GetValue();
    else
        return false;
}

// ---------- ClassInstance -----------

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod("__str__"))
        Call("__str__", {}, context)->Print(os, context);
    else
        os << this;
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if (!HasMethod(method, actual_args.size()))
        throw std::runtime_error(
            "no inplementation of " + method + " in " + cls_.GetName()
        );

    Closure closure;
    closure["self"] = ObjectHolder::Share(*this);

    const Method* method_ptr = cls_.GetMethod(method);
    for (size_t i = 0u; i < actual_args.size(); ++i)
        closure[method_ptr->formal_params[i]] = actual_args[i];

    return method_ptr->body->Execute(closure, context);
}

// ---------- Class -------------------

const Method* Class::GetMethod(const std::string& name) const {
    const auto it = std::find_if(
        methods_.begin(),
        methods_.end(),
        [&name](const Method& method) { return method.name == name; }
    );

    if (it == methods_.end() && !parent_)
        return nullptr;
    else if (it == methods_.end() && parent_)
        return parent_->GetMethod(name);

    return &(*it);
}

void Bool::Print(std::ostream& os, Context&) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs,
           const ObjectHolder& rhs,
           Context& context) {
    {
        const Number* lhs_ptr = lhs.TryAs<Number>();
        const Number* rhs_ptr = rhs.TryAs<Number>();
        if (lhs_ptr && rhs_ptr)
            return lhs_ptr->GetValue() == rhs_ptr->GetValue();
    }
    {
        const String* lhs_ptr = lhs.TryAs<String>();
        const String* rhs_ptr = rhs.TryAs<String>();
        if (lhs_ptr && rhs_ptr)
            return lhs_ptr->GetValue() == rhs_ptr->GetValue();
    }
    {
        const Bool* lhs_ptr = lhs.TryAs<Bool>();
        const Bool* rhs_ptr = rhs.TryAs<Bool>();
        if (lhs_ptr && rhs_ptr)
            return lhs_ptr->GetValue() == rhs_ptr->GetValue();
    }

    ClassInstance* instance_ptr = lhs.TryAs<ClassInstance>();
    if (instance_ptr && instance_ptr->HasMethod("__eq__", 1u))
        return instance_ptr->Call("__eq__", {rhs}, context)
            .TryAs<Bool>()->GetValue();

    if (!bool(lhs) && !bool(rhs))
        return true;

    throw std::runtime_error("no viable equal operator");
}

bool Less(const ObjectHolder& lhs,
          const ObjectHolder& rhs,
          Context& context) {
    {
        const Number* lhs_ptr = lhs.TryAs<Number>();
        const Number* rhs_ptr = rhs.TryAs<Number>();
        if (lhs_ptr && rhs_ptr)
            return lhs_ptr->GetValue() < rhs_ptr->GetValue();
    }
    {
        const String* lhs_ptr = lhs.TryAs<String>();
        const String* rhs_ptr = rhs.TryAs<String>();
        if (lhs_ptr && rhs_ptr)
            return lhs_ptr->GetValue() < rhs_ptr->GetValue();
    }
    {
        const Bool* lhs_ptr = lhs.TryAs<Bool>();
        const Bool* rhs_ptr = rhs.TryAs<Bool>();
        if (lhs_ptr && rhs_ptr)
            return lhs_ptr->GetValue() < rhs_ptr->GetValue();
    }

    ClassInstance* instance_ptr = lhs.TryAs<ClassInstance>();
    if (instance_ptr && instance_ptr->HasMethod("__lt__", 1u))
        return instance_ptr->Call("__lt__", {rhs}, context)
            .TryAs<Bool>()->GetValue();

    throw std::runtime_error("no viable comparator");
}

}  // namespace runtime