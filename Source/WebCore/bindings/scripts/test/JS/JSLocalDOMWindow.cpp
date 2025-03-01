/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSLocalDOMWindow.h"

#include "ActiveDOMObject.h"
#include "ExtendedDOMClientIsoSubspaces.h"
#include "ExtendedDOMIsoSubspaces.h"
#include "JSDOMAttribute.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructorNotConstructable.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMWrapperCache.h"
#include "JSExposedStar.h"
#include "JSExposedToWorkerAndWindow.h"
#include "JSLocalDOMWindow.h"
#include "JSTestConditionalIncludes.h"
#include "JSTestConditionallyReadWrite.h"
#include "JSTestDefaultToJSON.h"
#include "JSTestDefaultToJSONFilteredByExposed.h"
#include "JSTestEnabledBySetting.h"
#include "JSTestEnabledForContext.h"
#include "JSTestNode.h"
#include "JSTestObj.h"
#include "JSTestPromiseRejectionEvent.h"
#include "JSWindowProxy.h"
#include "ScriptExecutionContext.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/HeapAnalyzer.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/SlotVisitorMacros.h>
#include <JavaScriptCore/SubspaceInlines.h>
#include <wtf/GetPtr.h>
#include <wtf/PointerPreparations.h>
#include <wtf/URL.h>

#if ENABLE(Condition1) || ENABLE(Condition2)
#include "JSTestInterface.h"
#endif


namespace WebCore {
using namespace JSC;

// Attributes

static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindowConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_ExposedStarConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_ExposedToWorkerAndWindowConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_LocalDOMWindowConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestConditionalIncludesConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestConditionallyReadWriteConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestDefaultToJSONConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestDefaultToJSONFilteredByExposedConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestEnabledBySettingConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestEnabledForContextConstructor);
#if ENABLE(Condition1) || ENABLE(Condition2)
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestInterfaceConstructor);
#endif
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestNodeConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestObjectConstructor);
static JSC_DECLARE_CUSTOM_GETTER(jsLocalDOMWindow_TestPromiseRejectionEventConstructor);

using JSLocalDOMWindowDOMConstructor = JSDOMConstructorNotConstructable<JSLocalDOMWindow>;

/* Hash table */

static const struct CompactHashIndex JSLocalDOMWindowTableIndex[38] = {
    { -1, -1 },
    { 2, 33 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 5, 34 },
    { -1, -1 },
    { 3, -1 },
    { 1, 32 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 0, -1 },
    { -1, -1 },
    { 10, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 4, 35 },
    { 6, 36 },
    { 7, 37 },
    { 8, -1 },
    { 9, -1 },
    { 11, -1 },
};


static const HashTableValue JSLocalDOMWindowTableValues[] =
{
    { "ExposedStar"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_ExposedStarConstructor, 0 } },
    { "ExposedToWorkerAndWindow"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_ExposedToWorkerAndWindowConstructor, 0 } },
    { "LocalDOMWindow"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_LocalDOMWindowConstructor, 0 } },
    { "TestConditionalIncludes"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestConditionalIncludesConstructor, 0 } },
    { "TestConditionallyReadWrite"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestConditionallyReadWriteConstructor, 0 } },
    { "TestDefaultToJSON"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestDefaultToJSONConstructor, 0 } },
    { "TestDefaultToJSONFilteredByExposed"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestDefaultToJSONFilteredByExposedConstructor, 0 } },
    { "TestEnabledBySetting"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestEnabledBySettingConstructor, 0 } },
#if ENABLE(Condition1) || ENABLE(Condition2)
    { "TestInterface"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestInterfaceConstructor, 0 } },
#else
    { { }, 0, NoIntrinsic, { HashTableValue::End } },
#endif
    { "TestNode"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestNodeConstructor, 0 } },
    { "TestObject"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestObjectConstructor, 0 } },
    { "TestPromiseRejectionEvent"_s, static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindow_TestPromiseRejectionEventConstructor, 0 } },
};

static const HashTable JSLocalDOMWindowTable = { 12, 31, static_cast<uint8_t>(static_cast<unsigned>(JSC::PropertyAttribute::DontEnum)), JSLocalDOMWindow::info(), JSLocalDOMWindowTableValues, JSLocalDOMWindowTableIndex };
template<> const ClassInfo JSLocalDOMWindowDOMConstructor::s_info = { "LocalDOMWindow"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSLocalDOMWindowDOMConstructor) };

template<> JSValue JSLocalDOMWindowDOMConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    return JSEventTarget::getConstructor(vm, &globalObject);
}

template<> void JSLocalDOMWindowDOMConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->length, jsNumber(0), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    JSString* nameString = jsNontrivialString(vm, "LocalDOMWindow"_s);
    m_originalName.set(vm, this, nameString);
    putDirect(vm, vm.propertyNames->name, nameString, JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->prototype, globalObject.getPrototypeDirect(), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum | JSC::PropertyAttribute::DontDelete);
}

/* Hash table for prototype */

static const struct CompactHashIndex JSLocalDOMWindowPrototypeTableIndex[2] = {
    { -1, -1 },
    { 0, -1 },
};


static const HashTableValue JSLocalDOMWindowPrototypeTableValues[] =
{
    { "constructor"_s, static_cast<unsigned>(PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::GetterSetterType, jsLocalDOMWindowConstructor, 0 } },
};

static const HashTable JSLocalDOMWindowPrototypeTable = { 1, 1, static_cast<uint8_t>(static_cast<unsigned>(PropertyAttribute::DontEnum)), JSLocalDOMWindow::info(), JSLocalDOMWindowPrototypeTableValues, JSLocalDOMWindowPrototypeTableIndex };
const ClassInfo JSLocalDOMWindowPrototype::s_info = { "LocalDOMWindow"_s, &Base::s_info, &JSLocalDOMWindowPrototypeTable, nullptr, CREATE_METHOD_TABLE(JSLocalDOMWindowPrototype) };

void JSLocalDOMWindowPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSLocalDOMWindow::info(), JSLocalDOMWindowPrototypeTableValues, *this);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

const ClassInfo JSLocalDOMWindow::s_info = { "LocalDOMWindow"_s, &Base::s_info, &JSLocalDOMWindowTable, nullptr, CREATE_METHOD_TABLE(JSLocalDOMWindow) };

JSLocalDOMWindow::JSLocalDOMWindow(VM& vm, Structure* structure, Ref<LocalDOMWindow>&& impl, JSWindowProxy* proxy)
    : JSEventTarget(vm, structure, WTFMove(impl), proxy)
{
}

void JSLocalDOMWindow::finishCreation(VM& vm, JSWindowProxy* proxy)
{
    Base::finishCreation(vm, proxy);

    static_assert(!std::is_base_of<ActiveDOMObject, LocalDOMWindow>::value, "Interface is not marked as [ActiveDOMObject] even though implementation class subclasses ActiveDOMObject.");

    if ((jsCast<JSDOMGlobalObject*>(globalObject())->scriptExecutionContext()->isSecureContext() && TestEnabledForContext::enabledForContext(*jsCast<JSDOMGlobalObject*>(globalObject())->scriptExecutionContext())))
        putDirectCustomAccessor(vm, builtinNames(vm).TestEnabledForContextPublicName(), CustomGetterSetter::create(vm, jsLocalDOMWindow_TestEnabledForContextConstructor, nullptr), attributesForStructure(static_cast<unsigned>(JSC::PropertyAttribute::DontEnum)));
}

JSValue JSLocalDOMWindow::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSLocalDOMWindowDOMConstructor, DOMConstructorID::LocalDOMWindow>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindowConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = JSC::getVM(lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicCast<JSLocalDOMWindowPrototype*>(JSValue::decode(thisValue));
    if (UNLIKELY(!prototype))
        return throwVMTypeError(lexicalGlobalObject, throwScope);
    return JSValue::encode(JSLocalDOMWindow::getConstructor(JSC::getVM(lexicalGlobalObject), prototype->globalObject()));
}

static inline JSValue jsLocalDOMWindow_ExposedStarConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSExposedStar::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_ExposedStarConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_ExposedStarConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_ExposedToWorkerAndWindowConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSExposedToWorkerAndWindow::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_ExposedToWorkerAndWindowConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_ExposedToWorkerAndWindowConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_LocalDOMWindowConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSLocalDOMWindow::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_LocalDOMWindowConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_LocalDOMWindowConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestConditionalIncludesConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestConditionalIncludes::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestConditionalIncludesConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestConditionalIncludesConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestConditionallyReadWriteConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestConditionallyReadWrite::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestConditionallyReadWriteConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestConditionallyReadWriteConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestDefaultToJSONConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestDefaultToJSON::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestDefaultToJSONConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestDefaultToJSONConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestDefaultToJSONFilteredByExposedConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestDefaultToJSONFilteredByExposed::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestDefaultToJSONFilteredByExposedConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestDefaultToJSONFilteredByExposedConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestEnabledBySettingConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestEnabledBySetting::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestEnabledBySettingConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestEnabledBySettingConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestEnabledForContextConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestEnabledForContext::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestEnabledForContextConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestEnabledForContextConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

#if ENABLE(Condition1) || ENABLE(Condition2)
static inline JSValue jsLocalDOMWindow_TestInterfaceConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestInterface::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestInterfaceConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestInterfaceConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

#endif

static inline JSValue jsLocalDOMWindow_TestNodeConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestNode::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestNodeConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestNodeConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestObjectConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestObj::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestObjectConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestObjectConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

static inline JSValue jsLocalDOMWindow_TestPromiseRejectionEventConstructorGetter(JSGlobalObject& lexicalGlobalObject, JSLocalDOMWindow& thisObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    return JSTestPromiseRejectionEvent::getConstructor(JSC::getVM(&lexicalGlobalObject), &thisObject);
}

JSC_DEFINE_CUSTOM_GETTER(jsLocalDOMWindow_TestPromiseRejectionEventConstructor, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName attributeName))
{
    return IDLAttribute<JSLocalDOMWindow>::get<jsLocalDOMWindow_TestPromiseRejectionEventConstructorGetter>(*lexicalGlobalObject, thisValue, attributeName);
}

JSC::GCClient::IsoSubspace* JSLocalDOMWindow::subspaceForImpl(JSC::VM& vm)
{
    return WebCore::subspaceForImpl<JSLocalDOMWindow, UseCustomHeapCellType::Yes>(vm,
        [] (auto& spaces) { return spaces.m_clientSubspaceForLocalDOMWindow.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_clientSubspaceForLocalDOMWindow = std::forward<decltype(space)>(space); },
        [] (auto& spaces) { return spaces.m_subspaceForLocalDOMWindow.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_subspaceForLocalDOMWindow = std::forward<decltype(space)>(space); },
        [] (auto& server) -> JSC::HeapCellType& { return server.m_heapCellTypeForJSLocalDOMWindow; }
    );
}

void JSLocalDOMWindow::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSLocalDOMWindow*>(cell);
    analyzer.setWrappedObjectForCell(cell, &thisObject->wrapped());
    if (thisObject->scriptExecutionContext())
        analyzer.setLabelForCell(cell, "url "_s + thisObject->scriptExecutionContext()->url().string());
    Base::analyzeHeap(cell, analyzer);
}

LocalDOMWindow* JSLocalDOMWindow::toWrapped(JSC::VM&, JSC::JSValue value)
{
    if (auto* wrapper = jsDynamicCast<JSLocalDOMWindow*>(value))
        return &wrapper->wrapped();
    return nullptr;
}

}
