#include "platform/WindowsBcdBackend.h"

#include <windows.h>
#include <wbemidl.h>

#include <memory>
#include <functional>

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {
namespace {

template<typename T>
struct ComReleaser {
    void operator()(T* value) const { if (value) value->Release(); }
};

template<typename T>
using ComPtr = std::unique_ptr<T, ComReleaser<T>>;

class Bstr final
{
public:
    explicit Bstr(const QString& value)
        : value_(SysAllocStringLen(reinterpret_cast<const OLECHAR*>(value.utf16()),
                                   static_cast<UINT>(value.size()))) {}
    explicit Bstr(const wchar_t* value) : value_(SysAllocString(value)) {}
    ~Bstr() { SysFreeString(value_); }
    operator BSTR() const { return value_; }
private:
    BSTR value_{};
};

QString errorText(HRESULT result)
{
    return u"BCD WMI: HRESULT 0x%1"_s.arg(
        static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
}

bool outputSucceeded(IWbemClassObject* output)
{
    if (!output) return false;
    VARIANT value;
    VariantInit(&value);
    const auto hr = output->Get(L"ReturnValue", 0, &value, nullptr, nullptr);
    const bool result = SUCCEEDED(hr)
        && ((value.vt == VT_BOOL && value.boolVal == VARIANT_TRUE)
            || ((value.vt == VT_I4 || value.vt == VT_UI4) && value.ulVal != 0));
    VariantClear(&value);
    return result;
}

ComPtr<IWbemClassObject> embeddedObject(IWbemClassObject* output, const wchar_t* property)
{
    if (!output) return {};
    VARIANT value;
    VariantInit(&value);
    if (FAILED(output->Get(property, 0, &value, nullptr, nullptr))) return {};
    IWbemClassObject* object{};
    if (value.vt == VT_UNKNOWN && value.punkVal) {
        value.punkVal->QueryInterface(IID_IWbemClassObject,
                                     reinterpret_cast<void**>(&object));
    } else if (value.vt == VT_DISPATCH && value.pdispVal) {
        value.pdispVal->QueryInterface(IID_IWbemClassObject,
                                      reinterpret_cast<void**>(&object));
    }
    VariantClear(&value);
    return ComPtr<IWbemClassObject>(object);
}

class WmiBcdSession final
{
public:
    WmiBcdSession()
    {
        const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        initialized_ = initialized == S_OK || initialized == S_FALSE;
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
            error_ = errorText(initialized);
            return;
        }
        IWbemLocator* locator{};
        auto hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IWbemLocator, reinterpret_cast<void**>(&locator));
        if (FAILED(hr)) { error_ = errorText(hr); return; }
        locator_.reset(locator);
        IWbemServices* services{};
        hr = locator_->ConnectServer(Bstr(L"ROOT\\WMI"), nullptr, nullptr, nullptr,
                                     0, nullptr, nullptr, &services);
        if (FAILED(hr)) { error_ = errorText(hr); return; }
        services_.reset(services);
        hr = CoSetProxyBlanket(services_.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                               nullptr, RPC_C_AUTHN_LEVEL_CALL,
                               RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) { error_ = errorText(hr); services_.reset(); }
    }

    ~WmiBcdSession()
    {
        services_.reset();
        locator_.reset();
        if (initialized_) CoUninitialize();
    }
    bool ready() const { return services_ != nullptr; }
    QString error() const { return error_; }

    ComPtr<IWbemClassObject> openObject(QStringView id, QString* error) const
    {
        const auto storeResult = executeClassMethod(
            u"BcdStore"_s, u"OpenStore"_s,
            [&](IWbemClassObject* input) {
                VARIANT file;
                VariantInit(&file);
                file.vt = VT_BSTR;
                file.bstrVal = SysAllocString(L"");
                const auto hr = input->Put(L"File", 0, &file, 0);
                VariantClear(&file);
                return hr;
            }, error);
        if (!storeResult || !outputSucceeded(storeResult.get())) return {};
        auto store = embeddedObject(storeResult.get(), L"Store");
        if (!store) { *error = u"BCD WMI не вернул хранилище."_s; return {}; }
        const auto objectResult = executeObjectMethod(
            store.get(), u"OpenObject"_s,
            [&](IWbemClassObject* input) {
                VARIANT value;
                VariantInit(&value);
                value.vt = VT_BSTR;
                value.bstrVal = SysAllocStringLen(
                    reinterpret_cast<const OLECHAR*>(id.utf16()),
                    static_cast<UINT>(id.size()));
                const auto hr = input->Put(L"Id", 0, &value, 0);
                VariantClear(&value);
                return hr;
            }, error);
        if (!objectResult || !outputSucceeded(objectResult.get())) return {};
        auto object = embeddedObject(objectResult.get(), L"Object");
        if (!object && error->isEmpty()) *error = u"BCD-объект отсутствует."_s;
        return object;
    }

    ComPtr<IWbemClassObject> getElement(
        IWbemClassObject* object, quint32 type, bool* missing, QString* error) const
    {
        const auto result = executeObjectMethod(
            object, u"GetElement"_s,
            [&](IWbemClassObject* input) {
                VARIANT value;
                VariantInit(&value);
                value.vt = VT_UI4;
                value.ulVal = type;
                return input->Put(L"Type", 0, &value, 0);
            }, error);
        if (!result) return {};
        if (!outputSucceeded(result.get())) {
            *missing = true;
            error->clear();
            return {};
        }
        auto element = embeddedObject(result.get(), L"Element");
        if (!element) *error = u"BCD WMI не вернул элемент."_s;
        return element;
    }

    BcdMutationResult mutate(
        IWbemClassObject* object, const QString& method,
        const std::function<HRESULT(IWbemClassObject*)>& fill) const
    {
        QString error;
        const auto output = executeObjectMethod(object, method, fill, &error);
        if (!output || !outputSucceeded(output.get())) {
            return {.error = error.isEmpty() ? u"BCD WMI отклонил изменение."_s : error};
        }
        return {.success = true};
    }

private:
    ComPtr<IWbemClassObject> executeClassMethod(
        const QString& className, const QString& method,
        const std::function<HRESULT(IWbemClassObject*)>& fill,
        QString* error) const
    {
        return executeMethod(className, className, method, fill, error);
    }

    ComPtr<IWbemClassObject> executeObjectMethod(
        IWbemClassObject* object, const QString& method,
        const std::function<HRESULT(IWbemClassObject*)>& fill,
        QString* error) const
    {
        VARIANT path;
        VARIANT className;
        VariantInit(&path);
        VariantInit(&className);
        if (FAILED(object->Get(L"__PATH", 0, &path, nullptr, nullptr))
            || path.vt != VT_BSTR
            || FAILED(object->Get(L"__CLASS", 0, &className, nullptr, nullptr))
            || className.vt != VT_BSTR) {
            VariantClear(&path);
            VariantClear(&className);
            *error = u"BCD WMI object path недоступен."_s;
            return {};
        }
        const auto pathText = QString::fromWCharArray(path.bstrVal);
        const auto classText = QString::fromWCharArray(className.bstrVal);
        VariantClear(&path);
        VariantClear(&className);
        return executeMethod(pathText, classText, method, fill, error);
    }

    ComPtr<IWbemClassObject> executeMethod(
        const QString& path, const QString& className, const QString& method,
        const std::function<HRESULT(IWbemClassObject*)>& fill,
        QString* error) const
    {
        IWbemClassObject* rawClass{};
        auto hr = services_->GetObject(Bstr(className), 0, nullptr, &rawClass, nullptr);
        if (FAILED(hr)) { *error = errorText(hr); return {}; }
        ComPtr<IWbemClassObject> classObject(rawClass);
        IWbemClassObject* rawInputDefinition{};
        hr = classObject->GetMethod(Bstr(method), 0, &rawInputDefinition, nullptr);
        if (FAILED(hr)) { *error = errorText(hr); return {}; }
        ComPtr<IWbemClassObject> inputDefinition(rawInputDefinition);
        IWbemClassObject* rawInput{};
        hr = inputDefinition->SpawnInstance(0, &rawInput);
        if (FAILED(hr)) { *error = errorText(hr); return {}; }
        ComPtr<IWbemClassObject> input(rawInput);
        hr = fill(input.get());
        if (FAILED(hr)) { *error = errorText(hr); return {}; }
        IWbemClassObject* rawOutput{};
        hr = services_->ExecMethod(Bstr(path), Bstr(method), 0, nullptr,
                                   input.get(), &rawOutput, nullptr);
        if (FAILED(hr)) { *error = errorText(hr); return {}; }
        return ComPtr<IWbemClassObject>(rawOutput);
    }

    bool initialized_{};
    ComPtr<IWbemLocator> locator_;
    ComPtr<IWbemServices> services_;
    QString error_;
};

BcdReadResult readValue(IWbemClassObject* element, domain::BcdValueKind kind)
{
    const wchar_t* property = kind == domain::BcdValueKind::Boolean ? L"Boolean"
        : kind == domain::BcdValueKind::Integer ? L"Integer" : L"String";
    VARIANT value;
    VariantInit(&value);
    const auto hr = element->Get(property, 0, &value, nullptr, nullptr);
    if (FAILED(hr)) return BcdReadResult::failed(errorText(hr));
    BcdReadResult result;
    if (kind == domain::BcdValueKind::Boolean && value.vt == VT_BOOL) {
        result = BcdReadResult::present(domain::BcdValue{value.boolVal == VARIANT_TRUE});
    } else if (kind == domain::BcdValueKind::Integer) {
        if (value.vt == VT_UI8) result = BcdReadResult::present(domain::BcdValue{value.ullVal});
        else if (value.vt == VT_BSTR) {
            bool ok{};
            const auto parsed = QString::fromWCharArray(value.bstrVal).toULongLong(&ok, 10);
            result = ok ? BcdReadResult::present(domain::BcdValue{parsed})
                        : BcdReadResult::failed(u"BCD integer имеет неверный формат."_s);
        } else result = BcdReadResult::failed(u"BCD integer имеет неверный тип."_s);
    } else if (kind == domain::BcdValueKind::String && value.vt == VT_BSTR) {
        result = BcdReadResult::present(domain::BcdValue{QString::fromWCharArray(value.bstrVal)});
    } else {
        result = BcdReadResult::failed(u"BCD-элемент имеет неожиданный тип."_s);
    }
    VariantClear(&value);
    return result;
}

} // namespace

BcdReadResult WindowsBcdBackend::read(const domain::BcdElementSpec& spec) const
{
    if (!domain::isWhitelistedBcdElement(spec)) return BcdReadResult::failed(u"BCD-объект или тип не входит в whitelist."_s);
    WmiBcdSession session;
    if (!session.ready()) return BcdReadResult::failed(session.error());
    QString error;
    auto object = session.openObject(spec.objectId, &error);
    if (!object) return BcdReadResult::failed(error);
    bool missing{};
    auto element = session.getElement(object.get(), spec.elementType, &missing, &error);
    if (missing) return BcdReadResult::missingElement();
    if (!element) return BcdReadResult::failed(error);
    return readValue(element.get(), spec.valueKind);
}

BcdMutationResult WindowsBcdBackend::set(
    const domain::BcdElementSpec& spec, const domain::BcdValue& value)
{
    if (!domain::isWhitelistedBcdElement(spec) || !domain::bcdValueMatchesKind(value, spec.valueKind)) {
        return {.error = u"BCD-объект, тип или значение не входит в whitelist."_s};
    }
    WmiBcdSession session;
    if (!session.ready()) return {.error = session.error()};
    QString error;
    auto object = session.openObject(spec.objectId, &error);
    if (!object) return {.error = error};
    const auto method = spec.valueKind == domain::BcdValueKind::Boolean
        ? u"SetBooleanElement"_s
        : spec.valueKind == domain::BcdValueKind::Integer
            ? u"SetIntegerElement"_s : u"SetStringElement"_s;
    return session.mutate(object.get(), method, [&](IWbemClassObject* input) {
        VARIANT type;
        VariantInit(&type);
        type.vt = VT_UI4;
        type.ulVal = spec.elementType;
        auto hr = input->Put(L"Type", 0, &type, 0);
        if (FAILED(hr)) return hr;
        VARIANT encoded;
        VariantInit(&encoded);
        const wchar_t* property{};
        if (const auto* boolean = std::get_if<bool>(&value)) {
            property = L"Boolean";
            encoded.vt = VT_BOOL;
            encoded.boolVal = *boolean ? VARIANT_TRUE : VARIANT_FALSE;
        } else if (const auto* integer = std::get_if<quint64>(&value)) {
            property = L"Integer";
            encoded.vt = VT_BSTR;
            encoded.bstrVal = SysAllocString(
                reinterpret_cast<const OLECHAR*>(QString::number(*integer).utf16()));
        } else {
            property = L"String";
            encoded.vt = VT_BSTR;
            encoded.bstrVal = SysAllocStringLen(
                reinterpret_cast<const OLECHAR*>(std::get<QString>(value).utf16()),
                static_cast<UINT>(std::get<QString>(value).size()));
        }
        hr = input->Put(property, 0, &encoded, 0);
        VariantClear(&encoded);
        return hr;
    });
}

BcdMutationResult WindowsBcdBackend::remove(const domain::BcdElementSpec& spec)
{
    if (!domain::isWhitelistedBcdElement(spec)) return {.error = u"BCD-объект или тип не входит в whitelist."_s};
    WmiBcdSession session;
    if (!session.ready()) return {.error = session.error()};
    QString error;
    auto object = session.openObject(spec.objectId, &error);
    if (!object) return {.error = error};
    return session.mutate(object.get(), u"DeleteElement"_s, [&](IWbemClassObject* input) {
        VARIANT type;
        VariantInit(&type);
        type.vt = VT_UI4;
        type.ulVal = spec.elementType;
        return input->Put(L"Type", 0, &type, 0);
    });
}

} // namespace tweakopedia::platform
