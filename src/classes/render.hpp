#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

namespace render
{
    class HardwareRenderer {
    private:
        ComPtr<ID2D1Factory> m_pD2DFactory;
        ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
        ComPtr<IDWriteFactory> m_pWriteFactory;
        ComPtr<IDWriteTextFormat> m_pTextFormat;

        // Brushes for different colors
        ComPtr<ID2D1SolidColorBrush> m_pBrush;

        bool m_initialized;
        HWND m_hWnd;

    public:
        HardwareRenderer() : m_initialized(false), m_hWnd(nullptr) {}

        ~HardwareRenderer() {
            Cleanup();
        }

        HRESULT Initialize(HWND hWnd) {
            HRESULT hr = S_OK;
            m_hWnd = hWnd;

            // Create D2D1 Factory
            hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&m_pD2DFactory));
            if (FAILED(hr)) return hr;

            // Create DWrite Factory
            hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(m_pWriteFactory),
                reinterpret_cast<IUnknown**>(m_pWriteFactory.GetAddressOf()));
            if (FAILED(hr)) return hr;

            // Create render target
            RECT rc;
            GetClientRect(hWnd, &rc);

            D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

            // Use IGNORE alpha mode for overlay transparency compatibility
            D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_HARDWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
                0, 0, // Default DPI
                D2D1_RENDER_TARGET_USAGE_NONE,
                D2D1_FEATURE_LEVEL_DEFAULT
            );

            D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRtProps = D2D1::HwndRenderTargetProperties(
                hWnd,
                size,
                D2D1_PRESENT_OPTIONS_NONE
            );

            hr = m_pD2DFactory->CreateHwndRenderTarget(
                rtProps,
                hwndRtProps,
                &m_pRenderTarget);

            if (FAILED(hr)) return hr;

            // Create default brush
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::White),
                &m_pBrush);

            if (FAILED(hr)) return hr;

            // Create default text format
            hr = m_pWriteFactory->CreateTextFormat(
                L"Arial",
                nullptr,
                DWRITE_FONT_WEIGHT_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.0f,
                L"",
                &m_pTextFormat);

            if (FAILED(hr)) return hr;

            // Set single line rendering
            m_pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            m_initialized = true;
            return hr;
        }

        void Cleanup() {
            m_pBrush.Reset();
            m_pTextFormat.Reset();
            m_pWriteFactory.Reset();
            m_pRenderTarget.Reset();
            m_pD2DFactory.Reset();
            m_initialized = false;
        }

        bool IsInitialized() const { return m_initialized; }

        void BeginDraw() {
            if (m_pRenderTarget) {
                m_pRenderTarget->BeginDraw();
                // Clear with white to match the colorkey transparency
                m_pRenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }

        HRESULT EndDraw() {
            if (m_pRenderTarget) {
                HRESULT hr = m_pRenderTarget->EndDraw();

                // Handle device lost scenarios
                if (hr == D2DERR_RECREATE_TARGET) {
                    CreateDeviceResources();
                    hr = S_OK;
                }
                return hr;
            }
            return E_FAIL;
        }

        void Resize(UINT width, UINT height) {
            if (m_pRenderTarget) {
                m_pRenderTarget->Resize(D2D1::SizeU(width, height));
            }
        }

        void DrawLine(int x1, int y1, int x2, int y2, COLORREF color) {
            if (!m_pRenderTarget) return;
            SetBrushColor(color);
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(static_cast<float>(x1), static_cast<float>(y1)),
                D2D1::Point2F(static_cast<float>(x2), static_cast<float>(y2)),
                m_pBrush.Get(),
                2.0f
            );
        }

        void DrawCircle(int x, int y, int radius, COLORREF color) {
            if (!m_pRenderTarget) return;
            SetBrushColor(color);
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)),
                static_cast<float>(radius),
                static_cast<float>(radius)
            );
            m_pRenderTarget->DrawEllipse(ellipse, m_pBrush.Get(), 2.0f);
        }

        void DrawBorderBox(int x, int y, int w, int h, COLORREF borderColor) {
            if (!m_pRenderTarget) return;
            SetBrushColor(borderColor);
            D2D1_RECT_F rect = D2D1::RectF(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(x + w),
                static_cast<float>(y + h)
            );
            m_pRenderTarget->DrawRectangle(rect, m_pBrush.Get(), 2.0f);
        }

        void DrawFilledBox(int x, int y, int width, int height, COLORREF color) {
            if (!m_pRenderTarget) return;
            SetBrushColor(color);
            D2D1_RECT_F rect = D2D1::RectF(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(x + width),
                static_cast<float>(y + height)
            );
            m_pRenderTarget->FillRectangle(rect, m_pBrush.Get());
        }

        void RenderText(int x, int y, const char* text, COLORREF textColor, int textSize) {
            if (!m_pRenderTarget || !text) return;

            // Convert char* to wstring
            int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
            if (len <= 0) return;

            std::wstring wideText(len - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text, -1, &wideText[0], len);

            // Create text format with specified size
            ComPtr<IDWriteTextFormat> textFormat;
            HRESULT hr = m_pWriteFactory->CreateTextFormat(
                L"Arial",
                nullptr,
                DWRITE_FONT_WEIGHT_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                static_cast<float>(textSize),
                L"",
                &textFormat);

            if (FAILED(hr)) return;

            // Force single line rendering with multiple settings
            textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

            // Create text layout for better control
            ComPtr<IDWriteTextLayout> textLayout;
            hr = m_pWriteFactory->CreateTextLayout(
                wideText.c_str(),
                static_cast<UINT32>(wideText.length()),
                textFormat.Get(),
                10000.0f,  // Very wide max width to prevent wrapping
                static_cast<float>(textSize + 10),  // Height
                &textLayout);

            if (FAILED(hr)) {
                // Fallback to original method if layout creation fails
                textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

                // Simple text outline for readability
                // Draw black outline
                m_pBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
                for (int ox = -1; ox <= 1; ox++) {
                    for (int oy = -1; oy <= 1; oy++) {
                        if (ox == 0 && oy == 0) continue;

                        D2D1_RECT_F layoutRect = D2D1::RectF(
                            static_cast<float>(x + ox),
                            static_cast<float>(y + oy),
                            static_cast<float>(x + ox + 10000),  // Much wider
                            static_cast<float>(y + oy + textSize + 10)
                        );

                        m_pRenderTarget->DrawText(
                            wideText.c_str(),
                            static_cast<UINT32>(wideText.length()),
                            textFormat.Get(),
                            layoutRect,
                            m_pBrush.Get()
                        );
                    }
                }

                // Draw main text
                SetBrushColor(textColor);
                D2D1_RECT_F layoutRect = D2D1::RectF(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    static_cast<float>(x + 10000),  // Much wider
                    static_cast<float>(y + textSize + 10)
                );

                m_pRenderTarget->DrawText(
                    wideText.c_str(),
                    static_cast<UINT32>(wideText.length()),
                    textFormat.Get(),
                    layoutRect,
                    m_pBrush.Get()
                );
                return;
            }

            // Use text layout for better control
            // Draw black outline
            m_pBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
            for (int ox = -1; ox <= 1; ox++) {
                for (int oy = -1; oy <= 1; oy++) {
                    if (ox == 0 && oy == 0) continue;

                    D2D1_POINT_2F origin = D2D1::Point2F(
                        static_cast<float>(x + ox),
                        static_cast<float>(y + oy)
                    );

                    m_pRenderTarget->DrawTextLayout(
                        origin,
                        textLayout.Get(),
                        m_pBrush.Get()
                    );
                }
            }

            // Draw main text
            SetBrushColor(textColor);
            D2D1_POINT_2F origin = D2D1::Point2F(
                static_cast<float>(x),
                static_cast<float>(y)
            );

            m_pRenderTarget->DrawTextLayout(
                origin,
                textLayout.Get(),
                m_pBrush.Get()
            );
        }

    private:
        void SetBrushColor(COLORREF color) {
            if (m_pBrush) {
                float r = GetRValue(color) / 255.0f;
                float g = GetGValue(color) / 255.0f;
                float b = GetBValue(color) / 255.0f;
                m_pBrush->SetColor(D2D1::ColorF(r, g, b, 1.0f));
            }
        }

        HRESULT CreateDeviceResources() {
            if (m_pRenderTarget) {
                m_pRenderTarget.Reset();
                m_pBrush.Reset();
            }

            RECT rc;
            GetClientRect(m_hWnd, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

            D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_HARDWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
                0, 0,
                D2D1_RENDER_TARGET_USAGE_NONE,
                D2D1_FEATURE_LEVEL_DEFAULT
            );

            D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRtProps = D2D1::HwndRenderTargetProperties(
                m_hWnd,
                size,
                D2D1_PRESENT_OPTIONS_NONE
            );

            HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
                rtProps,
                hwndRtProps,
                &m_pRenderTarget);

            if (SUCCEEDED(hr)) {
                hr = m_pRenderTarget->CreateSolidColorBrush(
                    D2D1::ColorF(D2D1::ColorF::White),
                    &m_pBrush);
            }

            return hr;
        }
    };

    // Global hardware renderer instance
    extern HardwareRenderer g_hwRenderer;

    // Main rendering functions that match your original API exactly
    inline void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
        if (g_hwRenderer.IsInitialized()) {
            g_hwRenderer.DrawLine(x1, y1, x2, y2, color);
        }
        else {
            // Your original GDI implementation
            HPEN hPen = CreatePen(PS_SOLID, 2, color);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, x1, y1, NULL);
            LineTo(hdc, x2, y2);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
        }
    }

    inline void DrawCircle(HDC hdc, int x, int y, int radius, COLORREF color) {
        if (g_hwRenderer.IsInitialized()) {
            g_hwRenderer.DrawCircle(x, y, radius, color);
        }
        else {
            // Your original GDI implementation
            HPEN hPen = CreatePen(PS_SOLID, 2, color);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            Arc(hdc, x - radius, y - radius, x + radius, y + radius * 1.5, 0, 0, 0, 0);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
        }
    }

    inline void DrawBorderBox(HDC hdc, int x, int y, int w, int h, COLORREF borderColor) {
        if (g_hwRenderer.IsInitialized()) {
            g_hwRenderer.DrawBorderBox(x, y, w, h, borderColor);
        }
        else {
            // Your original GDI implementation
            HBRUSH hBorderBrush = CreateSolidBrush(borderColor);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBorderBrush);
            RECT rect = { x, y, x + w, y + h };
            FrameRect(hdc, &rect, hBorderBrush);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hBorderBrush);
        }
    }

    inline void DrawFilledBox(HDC hdc, int x, int y, int width, int height, COLORREF color) {
        if (g_hwRenderer.IsInitialized()) {
            g_hwRenderer.DrawFilledBox(x, y, width, height, color);
        }
        else {
            // Your original GDI implementation
            HBRUSH hBrush = CreateSolidBrush(color);
            RECT rect = { x, y, x + width, y + height };
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }
    }

    inline void SetTextSize(HDC hdc, int textSize) {
        // Only needed for GDI fallback
        if (!g_hwRenderer.IsInitialized()) {
            LOGFONT lf;
            HFONT hFont, hOldFont;
            ZeroMemory(&lf, sizeof(LOGFONT));
            lf.lfHeight = -textSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = ANTIALIASED_QUALITY;
            hFont = CreateFontIndirect(&lf);
            hOldFont = (HFONT)SelectObject(hdc, hFont);
            DeleteObject(hOldFont);
        }
    }

    inline void RenderText(HDC hdc, int x, int y, const char* text, COLORREF textColor, int textSize) {
        if (g_hwRenderer.IsInitialized()) {
            g_hwRenderer.RenderText(x, y, text, textColor, textSize);
        }
        else {
            // Your original GDI implementation
            SetTextSize(hdc, textSize);
            SetTextColor(hdc, textColor);
            int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
            wchar_t* wide_text = new wchar_t[len];
            MultiByteToWideChar(CP_UTF8, 0, text, -1, wide_text, len);
            TextOutW(hdc, x, y, wide_text, len - 1);
            delete[] wide_text;
        }
    }
}