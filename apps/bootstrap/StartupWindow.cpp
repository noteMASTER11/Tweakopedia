#include "bootstrap/StartupWindow.h"

#include "startup/StartupIpc.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <objidl.h>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <utility>

namespace tweakopedia::bootstrap {
namespace {

constexpr wchar_t windowClassName[] = L"TweakopediaStartupWindow";
constexpr UINT stageMessage = WM_APP + 41;
constexpr UINT hideMessage = WM_APP + 42;
constexpr UINT animationTimer = 1;
constexpr int logoResourceId = 101;

float dpiScale(HWND window)
{
    return static_cast<float>(GetDpiForWindow(window)) / 96.0F;
}

Gdiplus::RectF centeredRect(float width, float y, float desiredWidth, float aspect)
{
    const auto height = desiredWidth / aspect;
    return {(width - desiredWidth) / 2.0F, y, desiredWidth, height};
}

}

void StartupWindowModel::advance(startup::StartupStage stage)
{
    if (!startup::isKnownStartupStage(stage) || complete_) return;
    if (stage == startup::StartupStage::Ready) {
        complete_ = true;
        return;
    }
    if (stage == startup::StartupStage::RuntimeExtract
        && std::find(pipeline_.begin(), pipeline_.end(), stage) == pipeline_.end()) {
        const auto cache = std::find(
            pipeline_.begin(), pipeline_.end(), startup::StartupStage::RuntimeCache);
        pipeline_.insert(cache == pipeline_.end() ? pipeline_.begin() : cache + 1, stage);
    }
    const auto next = std::find(pipeline_.begin(), pipeline_.end(), stage);
    if (next == pipeline_.end()) return;
    const auto current = std::find(pipeline_.begin(), pipeline_.end(), current_);
    if (started_ && current != pipeline_.end() && next < current) return;
    current_ = stage;
    started_ = true;
}

std::vector<startup::StartupStage> StartupWindowModel::visibleStages() const
{
    if (!started_ || complete_) return {};
    const auto current = std::find(pipeline_.begin(), pipeline_.end(), current_);
    if (current == pipeline_.end()) return {};
    const auto remaining = static_cast<std::size_t>(pipeline_.end() - current);
    const auto count = std::min<std::size_t>(3, remaining);
    return {current, current + static_cast<std::ptrdiff_t>(count)};
}

struct StartupWindow::Impl {
    HINSTANCE instance{};
    HWND window{};
    ULONG_PTR gdiplusToken{};
    IStream* logoStream{};
    std::unique_ptr<Gdiplus::Image> logo;
    StartupWindowModel model;
    float angle{-90.0F};

    ~Impl()
    {
        if (window && IsWindow(window)) DestroyWindow(window);
        logo.reset();
        if (logoStream) logoStream->Release();
        if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
    }

    bool loadLogo()
    {
        const auto resource = FindResourceW(
            instance, MAKEINTRESOURCEW(logoResourceId), MAKEINTRESOURCEW(10));
        if (!resource) return false;
        const auto loaded = LoadResource(instance, resource);
        const auto size = SizeofResource(instance, resource);
        const auto* bytes = static_cast<const std::byte*>(LockResource(loaded));
        if (!loaded || !bytes || size == 0) return false;

        const auto memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!memory) return false;
        auto* destination = GlobalLock(memory);
        if (!destination) {
            GlobalFree(memory);
            return false;
        }
        std::memcpy(destination, bytes, size);
        GlobalUnlock(memory);
        if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &logoStream))) {
            GlobalFree(memory);
            return false;
        }
        logo.reset(Gdiplus::Image::FromStream(logoStream, FALSE));
        return logo && logo->GetLastStatus() == Gdiplus::Ok;
    }

    void paint()
    {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        const auto pixelWidth = std::max<LONG>(1, client.right - client.left);
        const auto pixelHeight = std::max<LONG>(1, client.bottom - client.top);
        Gdiplus::Bitmap surface(pixelWidth, pixelHeight, PixelFormat32bppPARGB);
        Gdiplus::Graphics graphics(&surface);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        graphics.Clear(Gdiplus::Color(255, 247, 248, 252));

        const auto width = static_cast<float>(pixelWidth);
        const auto height = static_cast<float>(pixelHeight);
        const auto scale = dpiScale(window);
        const auto compact = height < 680.0F * scale;
        const auto logoWidth = std::min(width * 0.55F, (compact ? 470.0F : 560.0F) * scale);
        const auto logoY = std::max(50.0F * scale, height * (compact ? 0.10F : 0.14F));
        const auto aspect = logo && logo->GetHeight() > 0
            ? static_cast<float>(logo->GetWidth()) / static_cast<float>(logo->GetHeight())
            : 3.0F;
        const auto logoRect = centeredRect(width, logoY, logoWidth, aspect);
        if (logo) graphics.DrawImage(logo.get(), logoRect);

        Gdiplus::FontFamily family(L"Segoe UI");
        Gdiplus::Font heading(
            &family, (compact ? 30.0F : 34.0F) * scale,
            Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font status(
            &family, (compact ? 15.0F : 17.0F) * scale,
            Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::StringFormat centered;
        centered.SetAlignment(Gdiplus::StringAlignmentCenter);
        centered.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::SolidBrush headingBrush(Gdiplus::Color(255, 24, 36, 58));
        Gdiplus::SolidBrush currentBrush(Gdiplus::Color(255, 31, 43, 64));
        Gdiplus::SolidBrush pendingBrush(Gdiplus::Color(255, 113, 129, 154));
        Gdiplus::SolidBrush blueBrush(Gdiplus::Color(255, 0, 120, 212));
        Gdiplus::SolidBrush dotBrush(Gdiplus::Color(255, 210, 217, 227));

        const auto titleY = logoRect.GetBottom() + (compact ? 26.0F : 34.0F) * scale;
        const Gdiplus::RectF titleRect(0.0F, titleY, width, 52.0F * scale);
        graphics.DrawString(L"Загрузка...", -1, &heading, titleRect, &centered, &headingBrush);

        const auto spinnerSize = (compact ? 42.0F : 48.0F) * scale;
        const auto spinnerY = titleRect.GetBottom() + 12.0F * scale;
        const Gdiplus::RectF spinnerRect(
            (width - spinnerSize) / 2.0F, spinnerY, spinnerSize, spinnerSize);
        Gdiplus::Pen track(Gdiplus::Color(255, 215, 226, 240), 5.0F * scale);
        Gdiplus::Pen active(Gdiplus::Color(255, 0, 120, 212), 5.0F * scale);
        active.SetStartCap(Gdiplus::LineCapRound);
        active.SetEndCap(Gdiplus::LineCapRound);
        graphics.DrawEllipse(&track, spinnerRect);
        graphics.DrawArc(&active, spinnerRect, angle, 105.0F);

        const auto stages = model.visibleStages();
        const auto listWidth = std::min(width - 80.0F * scale, 560.0F * scale);
        const auto listX = (width - listWidth) / 2.0F;
        const auto rowHeight = (compact ? 32.0F : 38.0F) * scale;
        const auto desiredListY = spinnerRect.GetBottom() + 28.0F * scale;
        const auto listHeight = rowHeight * static_cast<float>(stages.size());
        const auto listY = std::min(
            desiredListY,
            std::max(spinnerRect.GetBottom() + 8.0F * scale,
                height - listHeight - 12.0F * scale));
        for (std::size_t index = 0; index < stages.size(); ++index) {
            const auto y = listY + static_cast<float>(index) * rowHeight;
            const auto dotSize = (index == 0 ? 10.0F : 9.0F) * scale;
            const auto dotY = y + (rowHeight - dotSize) / 2.0F;
            graphics.FillEllipse(index == 0 ? &blueBrush : &dotBrush,
                listX, dotY, dotSize, dotSize);
            const auto label = startup::startupStageText(stages[index]);
            Gdiplus::RectF textRect(
                listX + 28.0F * scale, y, listWidth - 28.0F * scale, rowHeight);
            Gdiplus::StringFormat format;
            format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            graphics.DrawString(label.data(), static_cast<INT>(label.size()), &status,
                textRect, &format, index == 0 ? &currentBrush : &pendingBrush);
        }
        Gdiplus::Graphics destination(dc);
        destination.DrawImage(&surface, 0, 0, pixelWidth, pixelHeight);
        EndPaint(window, &paint);
    }

    void applyStage(startup::StartupStage stage)
    {
        model.advance(stage);
        if (model.complete()) ShowWindow(window, SW_HIDE);
        InvalidateRect(window, nullptr, FALSE);
    }

    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<Impl*>(create->lpCreateParams);
            self->window = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(window, message, wParam, lParam);

        switch (message) {
        case WM_PAINT:
            self->paint();
            return 0;
        case WM_TIMER:
            if (wParam == animationTimer) {
                self->angle = std::fmod(self->angle + 7.0F, 360.0F);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case stageMessage:
            self->applyStage(static_cast<startup::StartupStage>(wParam));
            return 0;
        case hideMessage:
            ShowWindow(window, SW_HIDE);
            return 0;
        case WM_COPYDATA: {
            const auto* payload = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
            if (payload) {
                const auto stage = startup::decodeStartupMessage(payload->lpData, payload->cbData);
                if (stage) self->applyStage(*stage);
            }
            return TRUE;
        }
        case WM_DPICHANGED: {
            const auto* bounds = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(window, nullptr, bounds->left, bounds->top,
                bounds->right - bounds->left, bounds->bottom - bounds->top,
                SWP_NOACTIVATE | SWP_NOZORDER);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            const auto scale = dpiScale(window);
            limits->ptMinTrackSize.x = static_cast<LONG>(900.0F * scale);
            limits->ptMinTrackSize.y = static_cast<LONG>(620.0F * scale);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CLOSE:
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }
};

StartupWindow::StartupWindow() : impl_(std::make_unique<Impl>()) {}
StartupWindow::~StartupWindow() = default;

bool StartupWindow::create(HINSTANCE instance)
{
    impl_->instance = instance;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&impl_->gdiplusToken, &input, nullptr) != Gdiplus::Ok) {
        return false;
    }
    if (!impl_->loadLogo()) return false;

    WNDCLASSEXW windowClass{
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = Impl::windowProcedure,
        .hInstance = instance,
        .hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1)),
        .hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)),
        .lpszClassName = windowClassName,
        .hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(1)),
    };
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    constexpr DWORD style = WS_OVERLAPPEDWINDOW;
    RECT bounds{0, 0, 1180, 760};
    AdjustWindowRectEx(&bounds, style, FALSE, 0);
    impl_->window = CreateWindowExW(0, windowClassName, L"Tweakopedia", style,
        CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, instance, impl_.get());
    if (!impl_->window) return false;

    const COLORREF caption = RGB(0, 120, 212);
    const COLORREF captionText = RGB(255, 255, 255);
    constexpr DWORD captionAttribute = 35;
    constexpr DWORD captionTextAttribute = 36;
    (void)DwmSetWindowAttribute(
        impl_->window, captionAttribute, &caption, sizeof(caption));
    (void)DwmSetWindowAttribute(
        impl_->window, captionTextAttribute, &captionText, sizeof(captionText));
    SetTimer(impl_->window, animationTimer, 33, nullptr);
    return true;
}

void StartupWindow::show()
{
    if (!impl_->window) return;
    ShowWindow(impl_->window, SW_SHOW);
    UpdateWindow(impl_->window);
}

void StartupWindow::report(startup::StartupStage stage) const
{
    if (!impl_->window) return;
    if (stage == startup::StartupStage::Ready) {
        SendMessageW(impl_->window, stageMessage, static_cast<WPARAM>(stage), 0);
    } else {
        PostMessageW(impl_->window, stageMessage, static_cast<WPARAM>(stage), 0);
    }
}

void StartupWindow::hideSynchronously() const
{
    if (impl_->window) SendMessageW(impl_->window, hideMessage, 0, 0);
}

void StartupWindow::messageLoopUntil(HANDLE workerCompleted) const
{
    for (;;) {
        const auto result = MsgWaitForMultipleObjects(
            1, &workerCompleted, FALSE, INFINITE, QS_ALLINPUT);
        if (result == WAIT_OBJECT_0 || result == WAIT_FAILED) break;
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

std::uintptr_t StartupWindow::nativeHandle() const noexcept
{
    return reinterpret_cast<std::uintptr_t>(impl_->window);
}

} // namespace tweakopedia::bootstrap
