#include "stdafx.h"

#include <propsys.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <Module.h>

#include "DesktopNotification.h"

using Microsoft::WRL::ComPtr;

// Only the slice of the Windows.UI.Notifications ABI used here, so no WinRT SDK headers are needed (MinGW has none).
namespace {
    using RtString = struct HSTRING__*;
    struct EventToken {
        int64_t value;
    };

    struct RtInspectable : IUnknown {
        virtual HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetRuntimeClassName(RtString*) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetTrustLevel(int*) = 0;
    };

    struct XmlDocumentIO : RtInspectable {
        virtual HRESULT STDMETHODCALLTYPE LoadXml(RtString xml) = 0;
    };

    struct ToastNotification : RtInspectable {
        virtual HRESULT STDMETHODCALLTYPE get_Content(void**) = 0;
        virtual HRESULT STDMETHODCALLTYPE put_ExpirationTime(void*) = 0;
        virtual HRESULT STDMETHODCALLTYPE get_ExpirationTime(void**) = 0;
        virtual HRESULT STDMETHODCALLTYPE add_Dismissed(IUnknown* handler, EventToken* token) = 0;
        virtual HRESULT STDMETHODCALLTYPE remove_Dismissed(EventToken token) = 0;
        virtual HRESULT STDMETHODCALLTYPE add_Activated(IUnknown* handler, EventToken* token) = 0;
        virtual HRESULT STDMETHODCALLTYPE remove_Activated(EventToken token) = 0;
        virtual HRESULT STDMETHODCALLTYPE add_Failed(IUnknown* handler, EventToken* token) = 0;
        virtual HRESULT STDMETHODCALLTYPE remove_Failed(EventToken token) = 0;
    };

    struct ToastNotifier : RtInspectable {
        virtual HRESULT STDMETHODCALLTYPE Show(ToastNotification* notification) = 0;
        virtual HRESULT STDMETHODCALLTYPE Hide(ToastNotification* notification) = 0;
    };

    struct ToastNotificationManagerStatics : RtInspectable {
        virtual HRESULT STDMETHODCALLTYPE CreateToastNotifier(void**) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateToastNotifierWithId(RtString app_id, ToastNotifier** notifier) = 0;
    };

    struct ToastNotificationFactory : RtInspectable {
        virtual HRESULT STDMETHODCALLTYPE CreateToastNotification(RtInspectable* xml_document, ToastNotification** notification) = 0;
    };

    struct ToastFailedEventArgs : RtInspectable {
        virtual HRESULT STDMETHODCALLTYPE get_ErrorCode(HRESULT* error) = 0;
    };

    constexpr IID IID_XmlDocument = {0xf7f3a506, 0x1e87, 0x42d6, {0xbc, 0xfb, 0xb8, 0xc8, 0x09, 0xfa, 0x54, 0x94}};
    constexpr IID IID_XmlDocumentIO = {0x6cd0e74e, 0xee65, 0x4489, {0x9e, 0xbf, 0xca, 0x43, 0xe8, 0x7b, 0xa6, 0x37}};
    constexpr IID IID_ToastNotificationManagerStatics = {0x50ac103f, 0xd235, 0x4598, {0xbb, 0xef, 0x98, 0xfe, 0x4d, 0x1a, 0x3a, 0xd4}};
    constexpr IID IID_ToastNotificationFactory = {0x04124b20, 0x82c6, 0x4229, {0xb1, 0x09, 0xfd, 0x9e, 0xd4, 0x66, 0x2b, 0x53}};
    constexpr IID IID_ToastFailedEventArgs = {0x35176862, 0xcfd4, 0x44f8, {0xad, 0x64, 0xf5, 0x00, 0xfd, 0x89, 0x6c, 0x3b}};
    constexpr IID IID_ActivatedHandler = {0xab54de2d, 0x97d9, 0x5528, {0xb6, 0xad, 0x10, 0x5a, 0xfe, 0x15, 0x65, 0x30}};
    constexpr IID IID_DismissedHandler = {0x61c2402f, 0x0ed0, 0x5a18, {0xab, 0x69, 0x59, 0xf4, 0xaa, 0x99, 0xa3, 0x68}};
    constexpr IID IID_FailedHandler = {0x95e3e803, 0xc969, 0x5e3a, {0x97, 0x53, 0xea, 0x2a, 0xd2, 0x2a, 0x9a, 0x33}};
    constexpr IID IID_AgileObject = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};
    constexpr PROPERTYKEY PKEY_AppUserModelId = {{0x9f4c2855, 0x9f79, 0x4b39, {0xa8, 0xd0, 0xe1, 0xd4, 0x2d, 0xe1, 0xd5, 0xf3}}, 5};

    // Resolved at runtime rather than imported, so the dll still loads where combase has no WinRT exports.
    struct WinRtApi {
        HRESULT(WINAPI* get_activation_factory)(RtString, REFIID, void**) = nullptr;
        HRESULT(WINAPI* activate_instance)(RtString, RtInspectable**) = nullptr;
        HRESULT(WINAPI* create_string)(const wchar_t*, UINT32, RtString*) = nullptr;
        HRESULT(WINAPI* delete_string)(RtString) = nullptr;

        [[nodiscard]] bool loaded() const { return get_activation_factory && activate_instance && create_string && delete_string; }
    };

    const WinRtApi& Api()
    {
        static const WinRtApi api = [] {
            WinRtApi resolved;
            const auto combase = LoadLibraryW(L"combase.dll");
            resolved.get_activation_factory = GetModuleFunctionW<decltype(resolved.get_activation_factory)>(combase, "RoGetActivationFactory");
            resolved.activate_instance = GetModuleFunctionW<decltype(resolved.activate_instance)>(combase, "RoActivateInstance");
            resolved.create_string = GetModuleFunctionW<decltype(resolved.create_string)>(combase, "WindowsCreateString");
            resolved.delete_string = GetModuleFunctionW<decltype(resolved.delete_string)>(combase, "WindowsDeleteString");
            return resolved;
        }();
        return api;
    }

    class ScopedRtString {
    public:
        explicit ScopedRtString(const std::wstring& text) { Api().create_string(text.c_str(), static_cast<UINT32>(text.size()), &value); }
        ~ScopedRtString() { Api().delete_string(value); }
        ScopedRtString(const ScopedRtString&) = delete;
        ScopedRtString& operator=(const ScopedRtString&) = delete;
        [[nodiscard]] RtString get() const { return value; }

    private:
        RtString value = nullptr;
    };

    class ToastEventHandler final : public IUnknown {
    public:
        ToastEventHandler(const IID& handler_iid, std::function<void(IUnknown* args)> on_invoke)
            : iid(handler_iid), invoke(std::move(on_invoke)) {}

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
        {
            if (riid == IID_IUnknown || riid == IID_AgileObject || riid == iid) {
                *out = this;
                AddRef();
                return S_OK;
            }
            *out = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            const auto remaining = --references;
            if (!remaining) delete this;
            return remaining;
        }
        virtual HRESULT STDMETHODCALLTYPE Invoke(RtInspectable*, IUnknown* args)
        {
            // The callback may dismiss the toast, which drops the last other reference to this handler.
            const ComPtr<ToastEventHandler> keep_alive(this);
            const std::lock_guard lock(mutex);
            if (const auto target = invoke) target(args);
            return S_OK;
        }

        void Disconnect()
        {
            const std::lock_guard lock(mutex);
            invoke = nullptr;
        }

    private:
        const IID iid;
        std::function<void(IUnknown*)> invoke;
        std::recursive_mutex mutex;
        std::atomic<ULONG> references = 1;
    };

    std::wstring EscapeXml(const std::wstring& text)
    {
        std::wstring escaped;
        escaped.reserve(text.size());
        for (const auto c : text) {
            switch (c) {
                case L'&': escaped += L"&amp;"; break;
                case L'<': escaped += L"&lt;"; break;
                case L'>': escaped += L"&gt;"; break;
                case L'"': escaped += L"&quot;"; break;
                case L'\'': escaped += L"&apos;"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    }

    std::filesystem::path StartMenuShortcutPath(const std::wstring& app_name)
    {
        wchar_t appdata[MAX_PATH];
        const auto length = GetEnvironmentVariableW(L"APPDATA", appdata, _countof(appdata));
        if (!length || length >= _countof(appdata)) return {};
        return std::filesystem::path(appdata) / L"Microsoft" / L"Windows" / L"Start Menu" / L"Programs" / (app_name + L".lnk");
    }

    bool ShortcutHasAppUserModelId(IPropertyStore* properties, const std::wstring& app_user_model_id)
    {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (FAILED(properties->GetValue(PKEY_AppUserModelId, &value))) return false;
        const auto matches = value.vt == VT_LPWSTR && value.pwszVal && app_user_model_id == value.pwszVal;
        PropVariantClear(&value);
        return matches;
    }

    // Toasts from an unpackaged process only show when a Start Menu shortcut carries the same AppUserModelID.
    HRESULT EnsureStartMenuShortcut(const std::wstring& app_name, const std::wstring& app_user_model_id)
    {
        const auto shortcut_path = StartMenuShortcutPath(app_name);
        if (shortcut_path.empty()) return E_FAIL;

        ComPtr<IShellLinkW> shell_link;
        auto hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(shell_link.GetAddressOf()));
        ComPtr<IPersistFile> persist_file;
        ComPtr<IPropertyStore> properties;
        if (SUCCEEDED(hr)) hr = shell_link->QueryInterface(IID_PPV_ARGS(persist_file.GetAddressOf()));
        if (SUCCEEDED(hr)) hr = shell_link->QueryInterface(IID_PPV_ARGS(properties.GetAddressOf()));
        if (FAILED(hr)) return hr;

        const auto exists = std::filesystem::exists(shortcut_path);
        if (exists) {
            hr = persist_file->Load(shortcut_path.c_str(), STGM_READWRITE);
            if (FAILED(hr)) return hr;
            if (ShortcutHasAppUserModelId(properties.Get(), app_user_model_id)) return S_OK;
        }
        else {
            wchar_t exe_path[MAX_PATH];
            if (!GetModuleFileNameW(nullptr, exe_path, _countof(exe_path))) return E_FAIL;
            hr = shell_link->SetPath(exe_path);
            if (SUCCEEDED(hr)) hr = shell_link->SetWorkingDirectory(std::filesystem::path(exe_path).parent_path().c_str());
            if (FAILED(hr)) return hr;
        }

        PROPVARIANT value;
        PropVariantInit(&value);
        value.vt = VT_LPWSTR;
        value.pwszVal = const_cast<wchar_t*>(app_user_model_id.c_str());
        hr = properties->SetValue(PKEY_AppUserModelId, value);
        if (SUCCEEDED(hr)) hr = properties->Commit();
        if (SUCCEEDED(hr)) hr = persist_file->Save(shortcut_path.c_str(), TRUE);
        return hr;
    }

    template <typename Interface>
    HRESULT GetFactory(const wchar_t* runtime_class, const IID& iid, ComPtr<Interface>& factory)
    {
        const ScopedRtString class_name(runtime_class);
        return Api().get_activation_factory(class_name.get(), iid, reinterpret_cast<void**>(factory.ReleaseAndGetAddressOf()));
    }

    HRESULT CreateToastXml(const std::wstring& title, const std::wstring& message, ComPtr<RtInspectable>& xml_document)
    {
        const ScopedRtString class_name(L"Windows.Data.Xml.Dom.XmlDocument");
        ComPtr<RtInspectable> instance;
        auto hr = Api().activate_instance(class_name.get(), instance.GetAddressOf());
        ComPtr<XmlDocumentIO> xml_io;
        if (SUCCEEDED(hr)) hr = instance->QueryInterface(IID_XmlDocumentIO, reinterpret_cast<void**>(xml_io.GetAddressOf()));
        if (FAILED(hr)) return hr;

        const auto xml = L"<toast><visual><binding template=\"ToastText02\"><text id=\"1\">" + EscapeXml(title)
                         + L"</text><text id=\"2\">" + EscapeXml(message) + L"</text></binding></visual></toast>";
        hr = xml_io->LoadXml(ScopedRtString(xml).get());
        if (FAILED(hr)) return hr;
        return instance->QueryInterface(IID_XmlDocument, reinterpret_cast<void**>(xml_document.ReleaseAndGetAddressOf()));
    }
}

struct DesktopNotification::Impl {
    ComPtr<ToastNotifier> notifier;
    ComPtr<ToastNotification> notification;
    ComPtr<ToastEventHandler> activated;
    ComPtr<ToastEventHandler> dismissed;
    ComPtr<ToastEventHandler> failed;
    EventToken activated_token{};
    EventToken dismissed_token{};
    EventToken failed_token{};
};

DesktopNotification::DesktopNotification(std::unique_ptr<Impl> impl)
    : impl(std::move(impl)) {}

DesktopNotification::~DesktopNotification()
{
    for (const auto& handler : {impl->activated, impl->dismissed, impl->failed}) {
        if (handler) handler->Disconnect();
    }
    impl->notification->remove_Activated(impl->activated_token);
    impl->notification->remove_Dismissed(impl->dismissed_token);
    impl->notification->remove_Failed(impl->failed_token);
    impl->notifier->Hide(impl->notification.Get());
}

bool DesktopNotification::IsSupported()
{
    return Api().loaded();
}

bool DesktopNotification::RegisterApp(const std::wstring& app_name, const std::wstring& app_user_model_id)
{
    if (!IsSupported()) return false;
    if (FAILED(EnsureStartMenuShortcut(app_name, app_user_model_id))) return false;
    return SUCCEEDED(SetCurrentProcessExplicitAppUserModelID(app_user_model_id.c_str()));
}

std::unique_ptr<DesktopNotification> DesktopNotification::Show(const std::wstring& app_user_model_id, const std::wstring& title, const std::wstring& message, Handlers handlers)
{
    if (!IsSupported()) return nullptr;

    auto impl = std::make_unique<Impl>();
    ComPtr<ToastNotificationManagerStatics> manager;
    ComPtr<ToastNotificationFactory> factory;
    ComPtr<RtInspectable> xml_document;
    auto hr = GetFactory(L"Windows.UI.Notifications.ToastNotificationManager", IID_ToastNotificationManagerStatics, manager);
    if (SUCCEEDED(hr)) hr = manager->CreateToastNotifierWithId(ScopedRtString(app_user_model_id).get(), impl->notifier.GetAddressOf());
    if (SUCCEEDED(hr)) hr = GetFactory(L"Windows.UI.Notifications.ToastNotification", IID_ToastNotificationFactory, factory);
    if (SUCCEEDED(hr)) hr = CreateToastXml(title, message, xml_document);
    if (SUCCEEDED(hr)) hr = factory->CreateToastNotification(xml_document.Get(), impl->notification.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    impl->activated.Attach(new ToastEventHandler(IID_ActivatedHandler, [on_activated = std::move(handlers.activated)](IUnknown*) {
        if (on_activated) on_activated();
    }));
    impl->dismissed.Attach(new ToastEventHandler(IID_DismissedHandler, [on_dismissed = std::move(handlers.dismissed)](IUnknown*) {
        if (on_dismissed) on_dismissed();
    }));
    impl->failed.Attach(new ToastEventHandler(IID_FailedHandler, [on_failed = std::move(handlers.failed)](IUnknown* args) {
        HRESULT error = E_FAIL;
        ComPtr<ToastFailedEventArgs> failure;
        if (args && SUCCEEDED(args->QueryInterface(IID_ToastFailedEventArgs, reinterpret_cast<void**>(failure.GetAddressOf())))) {
            failure->get_ErrorCode(&error);
        }
        if (on_failed) on_failed(error);
    }));

    hr = impl->notification->add_Activated(impl->activated.Get(), &impl->activated_token);
    if (SUCCEEDED(hr)) hr = impl->notification->add_Dismissed(impl->dismissed.Get(), &impl->dismissed_token);
    if (SUCCEEDED(hr)) hr = impl->notification->add_Failed(impl->failed.Get(), &impl->failed_token);
    std::unique_ptr<DesktopNotification> shown(new DesktopNotification(std::move(impl)));
    if (FAILED(hr) || FAILED(shown->impl->notifier->Show(shown->impl->notification.Get()))) return nullptr;
    return shown;
}
