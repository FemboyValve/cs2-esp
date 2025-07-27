#pragma once

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

        // Cache for text formats to improve performance
        std::unordered_map<int, ComPtr<IDWriteTextFormat>> m_textFormatCache;
        
        // Thread safety
        std::mutex m_renderMutex;

        bool m_initialized;
        HWND m_hWnd;

    public:
        HardwareRenderer() : m_initialized(false), m_hWnd(nullptr) {}

        ~HardwareRenderer() {
            Cleanup();
        }

        HRESULT Initialize(HWND hWnd);

        void Cleanup() {
            std::lock_guard<std::mutex> lock(m_renderMutex);
            
            m_textFormatCache.clear();
            m_pBrush.Reset();
            m_pTextFormat.Reset();
            m_pWriteFactory.Reset();
            m_pRenderTarget.Reset();
            m_pD2DFactory.Reset();
            m_initialized = false;
        }

        bool IsInitialized() const { 
            return m_initialized; 
        }

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

        void RenderText(int x, int y, const char* text, COLORREF textColor, int textSize);

    private:
        ComPtr<IDWriteTextFormat> GetOrCreateTextFormat(int textSize);

        void RenderTextSimple(int x, int y, const wchar_t* text, UINT32 textLength, COLORREF textColor, int textSize);

        void RenderTextFallback(int x, int y, const wchar_t* text, UINT32 textLength,
            COLORREF textColor, int textSize, IDWriteTextFormat* textFormat);

        void RenderTextWithOutline(int x, int y, IDWriteTextLayout* textLayout, COLORREF textColor);

        void SetBrushColor(COLORREF color) {
            if (m_pBrush) {
                float r = GetRValue(color) / 255.0f;
                float g = GetGValue(color) / 255.0f;
                float b = GetBValue(color) / 255.0f;
                m_pBrush->SetColor(D2D1::ColorF(r, g, b, 1.0f));
            }
        }

        HRESULT CreateDeviceResources();
    };

    // Global hardware renderer instance
    extern HardwareRenderer g_hwRenderer;

    // Main rendering functions
    inline void DrawLine(int x1, int y1, int x2, int y2, COLORREF color) {
        g_hwRenderer.DrawLine(x1, y1, x2, y2, color);
    }

    inline void DrawCircle(int x, int y, int radius, COLORREF color) {
        g_hwRenderer.DrawCircle(x, y, radius, color);
    }

    inline void DrawBorderBox(int x, int y, int w, int h, COLORREF borderColor) {
        g_hwRenderer.DrawBorderBox(x, y, w, h, borderColor);
    }

    inline void DrawFilledBox(int x, int y, int width, int height, COLORREF color) {
        g_hwRenderer.DrawFilledBox(x, y, width, height, color);
    }

    inline void RenderText(int x, int y, const char* text, COLORREF textColor, int fontSize) {
        g_hwRenderer.RenderText(x, y, text, textColor, fontSize);
    }
}