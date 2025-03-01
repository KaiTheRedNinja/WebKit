/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TypeCheck.h"

#include "AST.h"
#include "ASTStringDumper.h"
#include "ASTVisitor.h"
#include "CompilationMessage.h"
#include "ContextProviderInlines.h"
#include "Overload.h"
#include "TypeStore.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>

namespace WGSL {

static constexpr bool shouldDumpInferredTypes = false;

class TypeChecker : public AST::Visitor, public ContextProvider<Type*> {
public:
    TypeChecker(ShaderModule&);

    std::optional<FailedCheck> check();

    // Declarations
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;
    void visit(AST::Function&) override;

    // Statements
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;
    void visit(AST::CompoundStatement&) override;

    // Expressions
    void visit(AST::Expression&) override;
    void visit(AST::FieldAccessExpression&) override;
    void visit(AST::IndexAccessExpression&) override;
    void visit(AST::BinaryExpression&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::CallExpression&) override;
    void visit(AST::UnaryExpression&) override;

    // Literal Expressions
    void visit(AST::BoolLiteral&) override;
    void visit(AST::Signed32Literal&) override;
    void visit(AST::Float32Literal&) override;
    void visit(AST::Unsigned32Literal&) override;
    void visit(AST::AbstractIntegerLiteral&) override;
    void visit(AST::AbstractFloatLiteral&) override;

    // Types
    void visit(AST::TypeName&) override;
    void visit(AST::ArrayTypeName&) override;
    void visit(AST::NamedTypeName&) override;
    void visit(AST::ParameterizedTypeName&) override;
    void visit(AST::ReferenceTypeName&) override;

private:
    void visitFunctionBody(AST::Function&);
    void visitStructMembers(AST::Structure&);
    void vectorFieldAccess(const Types::Vector&, AST::FieldAccessExpression&);

    template<typename... Arguments>
    void typeError(const SourceSpan&, Arguments&&...);

    enum class InferBottom : bool { No, Yes };
    template<typename... Arguments>
    void typeError(InferBottom, const SourceSpan&, Arguments&&...);

    Type* infer(AST::Expression&);
    Type* resolve(AST::TypeName&);
    void inferred(Type*);
    bool unify(Type*, Type*) WARN_UNUSED_RETURN;
    bool isBottom(Type*) const;
    std::optional<unsigned> extractInteger(AST::Expression&);
    Type* chooseOverload(const String&, const Vector<Type*>& valueArguments, const Vector<Type*>& typeArguments);

    ShaderModule& m_shaderModule;
    Type* m_inferredType { nullptr };

    TypeStore& m_types;
    Vector<Error> m_errors;
    // FIXME: maybe these should live in the context
    HashMap<String, Vector<OverloadCandidate>> m_overloadedOperations;
};

TypeChecker::TypeChecker(ShaderModule& shaderModule)
    : m_shaderModule(shaderModule)
    , m_types(shaderModule.types())
{
    introduceVariable(AST::Identifier::make("void"_s), m_types.voidType());
    introduceVariable(AST::Identifier::make("bool"_s), m_types.boolType());
    introduceVariable(AST::Identifier::make("i32"_s), m_types.i32Type());
    introduceVariable(AST::Identifier::make("u32"_s), m_types.u32Type());
    introduceVariable(AST::Identifier::make("f32"_s), m_types.f32Type());
    introduceVariable(AST::Identifier::make("sampler"_s), m_types.samplerType());

    // This file contains the declarations generated from `TypeDeclarations.rb`
#include "TypeDeclarations.h" // NOLINT
}

std::optional<FailedCheck> TypeChecker::check()
{
    // FIXME: fill in struct fields in a second pass since declarations might be
    // out of order
    for (auto& structure : m_shaderModule.structures())
        visit(structure);

    for (auto& structure : m_shaderModule.structures())
        visitStructMembers(structure);

    for (auto& variable : m_shaderModule.variables())
        visit(variable);

    for (auto& function : m_shaderModule.functions())
        visit(function);

    for (auto& function : m_shaderModule.functions())
        visitFunctionBody(function);

    if (shouldDumpInferredTypes) {
        for (auto& error : m_errors)
            dataLogLn(error);
    }

    if (m_errors.isEmpty())
        return std::nullopt;


    // FIXME: add support for warnings
    Vector<Warning> warnings { };
    return FailedCheck { WTFMove(m_errors), WTFMove(warnings) };
}

// Declarations
void TypeChecker::visit(AST::Structure& structure)
{
    Type* structType = m_types.structType(structure);
    introduceVariable(structure.name(), structType);
}

void TypeChecker::visitStructMembers(AST::Structure& structure)
{
    auto* const* type = readVariable(structure.name());
    ASSERT(type);
    ASSERT(std::holds_alternative<Types::Struct>(**type));

    auto& structType = std::get<Types::Struct>(**type);
    for (auto& member : structure.members()) {
        auto* memberType = resolve(member.type());
        auto result = structType.fields.add(member.name().id(), memberType);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
}

void TypeChecker::visit(AST::Variable& variable)
{
    Type* result = nullptr;
    if (variable.maybeTypeName())
        result = resolve(*variable.maybeTypeName());
    if (variable.maybeInitializer()) {
        auto* initializerType = infer(*variable.maybeInitializer());
        if (!result)
            result = initializerType;
        else if (!unify(result, initializerType))
            typeError(InferBottom::No, variable.span(), "cannot initialize var of type '", *result, "' with value of type '", *initializerType, "'");
    }
    introduceVariable(variable.name(), result);
}

void TypeChecker::visit(AST::Function& function)
{
    // FIXME: allocate and build function type fromp parameters and return type
    if (function.maybeReturnType())
        resolve(*function.maybeReturnType());
    Type* functionType = nullptr;
    introduceVariable(function.name(), functionType);
}

void TypeChecker::visitFunctionBody(AST::Function& function)
{
    ContextScope functionContext(this);

    for (auto& parameter : function.parameters()) {
        auto* parameterType = resolve(parameter.typeName());
        introduceVariable(parameter.name(), parameterType);
    }

    AST::Visitor::visit(function.body());
}

// Statements
void TypeChecker::visit(AST::AssignmentStatement& statement)
{
    auto* lhs = infer(statement.lhs());
    auto* rhs = infer(statement.rhs());
    if (!unify(lhs, rhs))
        typeError(InferBottom::No, statement.span(), "cannot assign value of type '", *rhs, "' to '", *lhs, "'");
}

void TypeChecker::visit(AST::ReturnStatement& statement)
{
    // FIXME: handle functions that return void
    auto* type = infer(*statement.maybeExpression());

    // FIXME: unify type with the curent function's return type
    UNUSED_PARAM(type);
}

void TypeChecker::visit(AST::CompoundStatement& statement)
{
    ContextScope blockScope(this);
    AST::Visitor::visit(statement);
}

// Expressions
void TypeChecker::visit(AST::Expression&)
{
    // NOTE: this should never be called directly, only through `resolve`, which
    // captures the inferred type
    ASSERT_NOT_REACHED();
}

void TypeChecker::visit(AST::FieldAccessExpression& access)
{
    auto* baseType = infer(access.base());
    if (isBottom(baseType)) {
        inferred(m_types.bottomType());
        return;
    }

    if (std::holds_alternative<Types::Struct>(*baseType)) {
        auto& structType = std::get<Types::Struct>(*baseType);
        auto it = structType.fields.find(access.fieldName().id());
        if (it == structType.fields.end()) {
            typeError(access.span(), "struct '", *baseType, "' does not have a member called '", access.fieldName(), "'");
            return;
        }
        inferred(it->value);
        return;
    }

    if (std::holds_alternative<Types::Vector>(*baseType)) {
        auto& vector = std::get<Types::Vector>(*baseType);
        vectorFieldAccess(vector, access);
        return;
    }

    typeError(access.span(), "invalid member access expression. Expected vector or struct, got '", *baseType, "'");
}

void TypeChecker::visit(AST::IndexAccessExpression& access)
{
    auto* base = infer(access.base());
    auto* index = infer(access.index());

    if (isBottom(base)) {
        inferred(m_types.bottomType());
        return;
    }

    if (!unify(index, m_types.i32Type()) && !unify(index, m_types.u32Type()) && !unify(index, m_types.abstractIntType())) {
        typeError(access.span(), "index must be of type 'i32' or 'u32', found: '", *index, "'");
        return;
    }

    if (std::holds_alternative<Types::Array>(*base)) {
        // FIXME: check bounds if index is constant
        auto& array = std::get<Types::Array>(*base);
        inferred(array.element);
        return;
    }

    // FIXME: Implement reference and matrix accesses
    typeError(access.span(), "cannot index type '", *base, "'");
}

void TypeChecker::visit(AST::BinaryExpression& binary)
{
    // FIXME: this needs to resolve overloads, not just unify both types
    auto* leftType = infer(binary.leftExpression());
    auto* rightType = infer(binary.rightExpression());
    auto* result = chooseOverload(toString(binary.operation()), { leftType, rightType }, { });
    if (result)
        inferred(result);
    else
        typeError(binary.span(), "no matching overload for operator ", toString(binary.operation()), " (", *leftType, ", ", *rightType, ")");
}

void TypeChecker::visit(AST::IdentifierExpression& identifier)
{
    auto* const* type = readVariable(identifier.identifier());
    if (type) {
        inferred(*type);
        return;
    }

    typeError(identifier.span(), "unknown identifier: '", identifier.identifier(), "'");
}

void TypeChecker::visit(AST::CallExpression& call)
{
    Vector<Type*> arguments;
    arguments.reserveInitialCapacity(call.arguments().size());
    for (auto& argument : call.arguments())
        arguments.append(infer(argument));

    auto& target = call.target();
    bool isNamedType = is<AST::NamedTypeName>(target);
    bool isParameterizedType = is<AST::ParameterizedTypeName>(target);
    if (isNamedType || isParameterizedType) {
        Vector<Type*> typeArguments;
        String targetName = [&]() -> String {
            if (isNamedType)
                return downcast<AST::NamedTypeName>(target).name();
            auto& parameterizedType = downcast<AST::ParameterizedTypeName>(target);
            typeArguments.append(resolve(parameterizedType.elementType()));
            return AST::ParameterizedTypeName::baseToString(parameterizedType.base());
        }();
        auto* result = chooseOverload(targetName, arguments, typeArguments);
        if (result)
            inferred(result);
        else {
            StringPrintStream valueArgumentsStream;
            bool first = true;
            for (auto* argument : arguments) {
                if (!first)
                    valueArgumentsStream.print(", ");
                first = false;
                valueArgumentsStream.print(*argument);
            }
            StringPrintStream typeArgumentsStream;
            first = true;
            if (typeArguments.size()) {
                typeArgumentsStream.print("<");
                for (auto* typeArgument : typeArguments) {
                    if (!first)
                        typeArgumentsStream.print(", ");
                    first = false;
                    typeArgumentsStream.print(*typeArgument);
                }
                typeArgumentsStream.print(">");
            }
            typeError(call.span(), "no matching overload for initializer ", targetName, typeArgumentsStream.toString(), "(", valueArgumentsStream.toString(), ")");
        }
        target.m_resolvedType = result;
        return;
    }

    // FIXME: add support for user-defined function calls
    auto* result = resolve(target);
    inferred(result);
}

void TypeChecker::visit(AST::UnaryExpression& unary)
{
    auto* argument = infer(unary.expression());
    auto* result = chooseOverload(toString(unary.operation()), { argument }, { });
    if (result)
        inferred(result);
    else
        typeError(unary.span(), "no matching overload for operator ", toString(unary.operation()), " (", *argument, ")");
}

// Literal Expressions
void TypeChecker::visit(AST::BoolLiteral&)
{
    inferred(m_types.boolType());
}

void TypeChecker::visit(AST::Signed32Literal&)
{
    inferred(m_types.i32Type());
}

void TypeChecker::visit(AST::Float32Literal&)
{
    inferred(m_types.f32Type());
}

void TypeChecker::visit(AST::Unsigned32Literal&)
{
    inferred(m_types.u32Type());
}

void TypeChecker::visit(AST::AbstractIntegerLiteral&)
{
    inferred(m_types.abstractIntType());
}

void TypeChecker::visit(AST::AbstractFloatLiteral&)
{
    inferred(m_types.abstractFloatType());
}

// Types
void TypeChecker::visit(AST::TypeName&)
{
    // NOTE: this should never be called directly, only through `resolve`, which
    // captures the inferred type
    ASSERT_NOT_REACHED();
}

void TypeChecker::visit(AST::ArrayTypeName& array)
{
    // FIXME: handle the case where there is no element type
    ASSERT(array.maybeElementType());

    auto* elementType = resolve(*array.maybeElementType());
    if (isBottom(elementType)) {
        inferred(m_types.bottomType());
        return;
    }

    std::optional<unsigned> size;
    if (array.maybeElementCount()) {
        size = extractInteger(*array.maybeElementCount());
        if (!size) {
            typeError(array.span(), "array count must evaluate to a constant integer expression or override variable");
            return;
        }
    }

    inferred(m_types.arrayType(elementType, size));
}

void TypeChecker::visit(AST::NamedTypeName& namedType)
{
    auto* const* type = readVariable(namedType.name());
    if (type) {
        inferred(*type);
        return;
    }

    typeError(namedType.span(), "unknown type: '", namedType.name(), "'");
}

void TypeChecker::visit(AST::ParameterizedTypeName& type)
{
    auto* elementType = resolve(type.elementType());
    if (isBottom(elementType))
        inferred(m_types.bottomType());
    else
        inferred(m_types.constructType(type.base(), elementType));
}

void TypeChecker::visit(AST::ReferenceTypeName&)
{
    // FIXME: we don't yet parse reference types
    ASSERT_NOT_REACHED();
}

// Private helpers
std::optional<unsigned> TypeChecker::extractInteger(AST::Expression& expression)
{
    switch (expression.kind()) {
    case AST::NodeKind::AbstractIntegerLiteral:
        return { static_cast<unsigned>(downcast<AST::AbstractIntegerLiteral>(expression).value()) };
    case AST::NodeKind::Unsigned32Literal:
        return { static_cast<unsigned>(downcast<AST::Unsigned32Literal>(expression).value()) };
    case AST::NodeKind::Signed32Literal:
        return { static_cast<unsigned>(downcast<AST::Signed32Literal>(expression).value()) };
    default:
        // FIXME: handle constants and overrides
        return std::nullopt;
    }
}

void TypeChecker::vectorFieldAccess(const Types::Vector& vector, AST::FieldAccessExpression& access)
{
    const auto& fieldName = access.fieldName().id();
    auto length = fieldName.length();

    const auto& isXYZW = [&](char c) {
        return c == 'x' || c == 'y' || c == 'z' || c == 'w';
    };
    const auto& isRGBA = [&](char c) {
        return c == 'r' || c == 'g' || c == 'b' || c == 'a';
    };

    bool hasXYZW = false;
    bool hasRGBA = false;
    for (unsigned i = 0; i < length; ++i) {
        char c = fieldName[i];
        if (isXYZW(c))
            hasXYZW = true;
        else if (isRGBA(c))
            hasRGBA = true;
        else {
            typeError(access.span(), "invalid vector swizzle character");
            return;
        }
    }

    if (hasXYZW && hasRGBA) {
        typeError(access.span(), "invalid vector swizzle member");
        return;
    }

    AST::ParameterizedTypeName::Base base;
    switch (length) {
    case 1:
        inferred(vector.element);
        return;
    case 2:
        base = AST::ParameterizedTypeName::Base::Vec2;
        break;
    case 3:
        base = AST::ParameterizedTypeName::Base::Vec3;
        break;
    case 4:
        base = AST::ParameterizedTypeName::Base::Vec4;
        break;
    default:
        typeError(access.span(), "invalid vector swizzle size");
        return;
    }

    inferred(m_types.constructType(base, vector.element));
}

Type* TypeChecker::chooseOverload(const String& operation, const Vector<Type*>& valueArguments, const Vector<Type*>& typeArguments)
{
    auto it = m_overloadedOperations.find(operation);
    if (it == m_overloadedOperations.end())
        return nullptr;
    for (auto* argument : valueArguments) {
        if (isBottom(argument))
            return m_types.bottomType();
    }
    return resolveOverloads(m_types, it->value, valueArguments, typeArguments);
}

Type* TypeChecker::infer(AST::Expression& expression)
{
    ASSERT(!m_inferredType);
    AST::Visitor::visit(expression);
    ASSERT(m_inferredType);

    if (shouldDumpInferredTypes) {
        dataLog("> Type inference [expression]: ");
        dumpNode(WTF::dataFile(), expression);
        dataLog(" : ");
        dataLogLn(*m_inferredType);
    }

    expression.m_inferredType = m_inferredType;
    Type* inferredType = m_inferredType;
    m_inferredType = nullptr;

    return inferredType;
}

Type* TypeChecker::resolve(AST::TypeName& type)
{
    ASSERT(!m_inferredType);
    AST::Visitor::visit(type);
    ASSERT(m_inferredType);

    if (shouldDumpInferredTypes) {
        dataLog("> Type inference [type]: ");
        dumpNode(WTF::dataFile(), type);
        dataLog(" : ");
        dataLogLn(*m_inferredType);
    }

    type.m_resolvedType = m_inferredType;
    Type* inferredType = m_inferredType;
    m_inferredType = nullptr;

    return inferredType;
}

void TypeChecker::inferred(Type* type)
{
    ASSERT(type);
    ASSERT(!m_inferredType);
    m_inferredType = type;
}

bool TypeChecker::unify(Type* lhs, Type* rhs)
{
    if (shouldDumpInferredTypes)
        dataLogLn("[unify] '", *lhs, "' <", RawPointer(lhs), ">  and '", *rhs, "' <", RawPointer(rhs), ">");

    if (lhs == rhs)
        return true;

    // Bottom is only inferred when a type error is reported, so we skip further
    // checks that are a consequence of an already reported error.
    if (isBottom(lhs) || isBottom(rhs))
        return true;

    return false;
}

bool TypeChecker::isBottom(Type* type) const
{
    return type == m_types.bottomType();
}

template<typename... Arguments>
void TypeChecker::typeError(const SourceSpan& span, Arguments&&... arguments)
{
    typeError(InferBottom::Yes, span, std::forward<Arguments>(arguments)...);
}

template<typename... Arguments>
void TypeChecker::typeError(InferBottom inferBottom, const SourceSpan& span, Arguments&&... arguments)
{
    m_errors.append({ makeString(std::forward<Arguments>(arguments)...), span });
    if (inferBottom == InferBottom::Yes)
        inferred(m_types.bottomType());
}

std::optional<FailedCheck> typeCheck(ShaderModule& shaderModule)
{
    return TypeChecker(shaderModule).check();
}

} // namespace WGSL
