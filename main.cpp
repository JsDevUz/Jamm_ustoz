// Jamm Host — Windows prototipi.
// Maqsad: veb doska bilan yonma-yon QALAM KECHIKISHINI solishtirish.
//   - Win32 + WM_POINTER (qalam bosimi, barcha oraliq nuqtalar = "history")
//   - Direct2D bilan chizish; "quruq" chiziqlar keshlanadi, hozirgi chiziq har nuqtada darhol chiziladi
//   - oppoq varaq, 1 ta qalam, 3 ta rang, 3 xil qalam uchi o'lchami
// Sinxronizatsiya, PDF, ovoz yo'q: bular keyingi bosqich.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "user32.lib")
#endif

namespace {

struct Pt { float x, y, p; };                 // x, y — DIP (qurilmadan mustaqil piksel), p — bosim 0..1
struct Sample { float x, y, p; uint64_t t; }; // x, y — mijoz pikseli
struct Stroke {
  D2D1_COLOR_F color;
  float size;
  bool pressure;
  std::vector<Pt> pts;
};

const D2D1_COLOR_F kColors[3] = {{0.08f, 0.08f, 0.09f, 1.f}, {0.10f, 0.36f, 0.88f, 1.f}, {0.87f, 0.16f, 0.16f, 1.f}};
const float kSizes[3] = {2.0f, 5.0f, 10.0f};  // qalam uchi o'lchami (DIP)
const float kToolbarH = 56.0f;

template <class T> void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

struct App {
  HWND hwnd = nullptr;
  UINT dpi = 96;
  ID2D1Factory* factory = nullptr;
  IDWriteFactory* dwrite = nullptr;
  IDWriteTextFormat* hudFmt = nullptr;
  ID2D1StrokeStyle* roundStyle = nullptr;
  ID2D1HwndRenderTarget* rt = nullptr;
  ID2D1BitmapRenderTarget* dry = nullptr;
  ID2D1SolidColorBrush* brush = nullptr;
  ID2D1SolidColorBrush* dryBrush = nullptr;

  int colorIdx = 0;
  int sizeIdx = 1;
  bool usePressure = true;

  std::vector<Stroke> strokes;
  Stroke cur{};
  UINT32 activeId = 0;

  // HUD
  std::wstring hudType = L"-";
  float hudPressure = 0.f;
  double rateHz = 0.0;
  double latMs = 0.0;   // ilova ichidagi kechikish: qurilma hodisasidan render tugashigacha
  UINT64 rateCount = 0;
  ULONGLONG rateStart = 0;
  LARGE_INTEGER freq{};
} g;

float ToDip(float px) { return px * 96.0f / static_cast<float>(g.dpi); }
float WidthDip() { RECT r; GetClientRect(g.hwnd, &r); return ToDip(static_cast<float>(r.right)); }
float HeightDip() { RECT r; GetClientRect(g.hwnd, &r); return ToDip(static_cast<float>(r.bottom)); }

// ---------- Resurslar ----------
void DiscardTargets() {
  SafeRelease(g.brush);
  SafeRelease(g.dryBrush);
  SafeRelease(g.dry);
  SafeRelease(g.rt);
}

void DrawStroke(ID2D1RenderTarget* t, ID2D1SolidColorBrush* b, const Stroke& s) {
  if (s.pts.empty()) return;
  b->SetColor(s.color);
  auto widthFor = [&](float p) { return s.pressure ? s.size * (0.35f + 0.65f * p) : s.size; };
  if (s.pts.size() == 1) {
    const float r = widthFor(s.pts[0].p) * 0.5f;
    t->FillEllipse(D2D1::Ellipse(D2D1::Point2F(s.pts[0].x, s.pts[0].y), r, r), b);
    return;
  }
  for (size_t i = 1; i < s.pts.size(); ++i) {
    const Pt& a = s.pts[i - 1];
    const Pt& c = s.pts[i];
    t->DrawLine(D2D1::Point2F(a.x, a.y), D2D1::Point2F(c.x, c.y), b, widthFor((a.p + c.p) * 0.5f), g.roundStyle);
  }
}

void RebuildDry() {
  if (!g.dry) return;
  g.dry->BeginDraw();
  g.dry->Clear(D2D1::ColorF(1.f, 1.f, 1.f, 0.f));
  for (const Stroke& s : g.strokes) DrawStroke(g.dry, g.dryBrush, s);
  g.dry->EndDraw();
}

bool CreateTargets() {
  if (g.rt) return true;
  RECT rc;
  GetClientRect(g.hwnd, &rc);
  const D2D1_SIZE_U size = D2D1::SizeU(std::max<LONG>(1, rc.right), std::max<LONG>(1, rc.bottom));
  // IMMEDIATELY: ekran yangilanishini (vsync) kutmasdan chiqarish: kechikishni kamaytiradi.
  if (FAILED(g.factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(g.hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY), &g.rt))) return false;
  g.rt->SetDpi(static_cast<float>(g.dpi), static_cast<float>(g.dpi));
  g.rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &g.brush);
  if (SUCCEEDED(g.rt->CreateCompatibleRenderTarget(&g.dry))) g.dry->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &g.dryBrush);
  RebuildDry();
  return g.brush && g.dry && g.dryBrush;
}

// ---------- Asboblar paneli ----------
D2D1_POINT_2F ColorCenter(int i) { return D2D1::Point2F(32.f + i * 44.f, kToolbarH * 0.5f); }
D2D1_POINT_2F SizeCenter(int i) { return D2D1::Point2F(210.f + i * 48.f, kToolbarH * 0.5f); }

bool HandleToolbarClick(float x, float y) {
  for (int i = 0; i < 3; ++i) {
    const D2D1_POINT_2F c = ColorCenter(i);
    if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= 18.f * 18.f) { g.colorIdx = i; return true; }
  }
  for (int i = 0; i < 3; ++i) {
    const D2D1_POINT_2F c = SizeCenter(i);
    if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= 20.f * 20.f) { g.sizeIdx = i; return true; }
  }
  return y < kToolbarH;
}

void DrawToolbar(float wDip) {
  ID2D1RenderTarget* t = g.rt;
  g.brush->SetColor(D2D1::ColorF(0.96f, 0.96f, 0.97f));
  t->FillRectangle(D2D1::RectF(0, 0, wDip, kToolbarH), g.brush);
  g.brush->SetColor(D2D1::ColorF(0.85f, 0.85f, 0.87f));
  t->FillRectangle(D2D1::RectF(0, kToolbarH - 1.f, wDip, kToolbarH), g.brush);

  for (int i = 0; i < 3; ++i) {
    const D2D1_POINT_2F c = ColorCenter(i);
    g.brush->SetColor(kColors[i]);
    t->FillEllipse(D2D1::Ellipse(c, 12.f, 12.f), g.brush);
    if (i == g.colorIdx) {
      g.brush->SetColor(D2D1::ColorF(0.25f, 0.25f, 0.28f));
      t->DrawEllipse(D2D1::Ellipse(c, 17.f, 17.f), g.brush, 2.f);
    }
  }
  for (int i = 0; i < 3; ++i) {
    const D2D1_POINT_2F c = SizeCenter(i);
    g.brush->SetColor(D2D1::ColorF(0.90f, 0.90f, 0.92f));
    t->FillEllipse(D2D1::Ellipse(c, 17.f, 17.f), g.brush);
    g.brush->SetColor(kColors[g.colorIdx]);
    t->FillEllipse(D2D1::Ellipse(c, kSizes[i] * 0.5f, kSizes[i] * 0.5f), g.brush);
    if (i == g.sizeIdx) {
      g.brush->SetColor(D2D1::ColorF(0.25f, 0.25f, 0.28f));
      t->DrawEllipse(D2D1::Ellipse(c, 19.f, 19.f), g.brush, 2.f);
    }
  }

  if (g.hudFmt) {
    wchar_t line[256];
    _snwprintf(line, 256, L"%ls   bosim %.2f   %.0f Hz   kechikish %.1f ms\n1-3 rang · Q/W/E uch · P bosim:%ls · Ctrl+Z · C tozalash",
               g.hudType.c_str(), g.hudPressure, g.rateHz, g.latMs, g.usePressure ? L"yoq" : L"o'chiq");
    g.brush->SetColor(D2D1::ColorF(0.35f, 0.35f, 0.38f));
    t->DrawText(line, static_cast<UINT32>(wcslen(line)), g.hudFmt, D2D1::RectF(wDip - 520.f, 8.f, wDip - 12.f, kToolbarH), g.brush);
  }
}

// ---------- Chizish ----------
void Render() {
  if (!CreateTargets()) return;
  const float wDip = WidthDip();
  const float hDip = HeightDip();
  g.rt->BeginDraw();
  g.rt->Clear(D2D1::ColorF(1.f, 1.f, 1.f));  // oppoq varaq
  ID2D1Bitmap* bmp = nullptr;
  if (SUCCEEDED(g.dry->GetBitmap(&bmp))) {
    g.rt->DrawBitmap(bmp, D2D1::RectF(0, 0, wDip, hDip));
    bmp->Release();
  }
  if (g.activeId) DrawStroke(g.rt, g.brush, g.cur);
  DrawToolbar(wDip);
  const HRESULT hr = g.rt->EndDraw();
  if (hr == static_cast<HRESULT>(D2DERR_RECREATE_TARGET)) DiscardTargets();
  ValidateRect(g.hwnd, nullptr);
}

// ---------- Kiritish (WM_POINTER) ----------
bool ClientPos(const POINTER_INFO& pi, bool isPen, float& x, float& y) {
  POINT origin{0, 0};
  ClientToScreen(g.hwnd, &origin);
  if (isPen) {
    // Yuqori aniqlik: qurilmaning xom himetrik koordinatasi -> ekran pikseli (kasr qiymat bilan).
    RECT dev{}, disp{};
    if (pi.sourceDevice && GetPointerDeviceRects(pi.sourceDevice, &dev, &disp) && dev.right > dev.left && dev.bottom > dev.top) {
      const double fx = disp.left + (static_cast<double>(pi.ptHimetricLocationRaw.x) - dev.left) * (disp.right - disp.left) / (dev.right - dev.left);
      const double fy = disp.top + (static_cast<double>(pi.ptHimetricLocationRaw.y) - dev.top) * (disp.bottom - disp.top) / (dev.bottom - dev.top);
      x = static_cast<float>(fx - origin.x);
      y = static_cast<float>(fy - origin.y);
      return true;
    }
  }
  x = static_cast<float>(pi.ptPixelLocation.x - origin.x);
  y = static_cast<float>(pi.ptPixelLocation.y - origin.y);
  return true;
}

// Bitta xabardagi BARCHA oraliq nuqtalarni (eskisidan yangisiga) yig'adi. Planshet sekundiga yuzlab
// nuqta beradi; faqat oxirgisini olish chiziqni siniq va "kechikkan" qiladi.
bool CollectSamples(UINT32 id, bool isPen, std::vector<Sample>& out) {
  out.clear();
  UINT32 n = 0;
  if (isPen) {
    if (!GetPointerPenInfoHistory(id, &n, nullptr) || n == 0) return false;
    std::vector<POINTER_PEN_INFO> h(n);
    if (!GetPointerPenInfoHistory(id, &n, h.data())) return false;
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
      Sample s{};
      ClientPos(h[i].pointerInfo, true, s.x, s.y);
      s.p = (h[i].penMask & PEN_MASK_PRESSURE) ? static_cast<float>(h[i].pressure) / 1024.f : 0.5f;
      s.t = h[i].pointerInfo.PerformanceCount;
      out.push_back(s);
    }
  } else {
    if (!GetPointerInfoHistory(id, &n, nullptr) || n == 0) return false;
    std::vector<POINTER_INFO> h(n);
    if (!GetPointerInfoHistory(id, &n, h.data())) return false;
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
      Sample s{};
      ClientPos(h[i], false, s.x, s.y);
      s.p = 1.f;
      s.t = h[i].PerformanceCount;
      out.push_back(s);
    }
  }
  return !out.empty();
}

void CountRate(size_t samples) {
  g.rateCount += samples;
  const ULONGLONG now = GetTickCount64();
  if (g.rateStart == 0) g.rateStart = now;
  if (now - g.rateStart >= 500) {
    g.rateHz = g.rateCount * 1000.0 / static_cast<double>(now - g.rateStart);
    g.rateCount = 0;
    g.rateStart = now;
  }
}

void UpdateLatency(uint64_t eventTicks) {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  if (eventTicks && now.QuadPart >= static_cast<LONGLONG>(eventTicks)) {
    const double ms = (now.QuadPart - static_cast<LONGLONG>(eventTicks)) * 1000.0 / static_cast<double>(g.freq.QuadPart);
    g.latMs = g.latMs == 0.0 ? ms : g.latMs * 0.9 + ms * 0.1;  // silliqlangan o'rtacha
  }
}

void AppendSamples(const std::vector<Sample>& v, bool isPen) {
  for (const Sample& s : v) {
    const Pt p{ToDip(s.x), ToDip(s.y), std::clamp(s.p, 0.f, 1.f)};
    if (!g.cur.pts.empty() && g.cur.pts.back().x == p.x && g.cur.pts.back().y == p.y) continue;
    g.cur.pts.push_back(p);
  }
  g.cur.pressure = g.usePressure && isPen;
}

void FinishStroke() {
  if (!g.activeId) return;
  g.activeId = 0;
  if (!g.cur.pts.empty()) {
    g.strokes.push_back(g.cur);
    if (g.dry) {
      g.dry->BeginDraw();
      DrawStroke(g.dry, g.dryBrush, g.strokes.back());
      g.dry->EndDraw();
    }
  }
  g.cur.pts.clear();
}

LRESULT HandlePointer(UINT msg, WPARAM wp) {
  const UINT32 id = GET_POINTERID_WPARAM(wp);
  POINTER_INPUT_TYPE type = PT_POINTER;
  if (!GetPointerType(id, &type)) return 0;
  const bool isPen = (type == PT_PEN);
  std::vector<Sample> samples;

  switch (msg) {
    case WM_POINTERDOWN: {
      if (!CollectSamples(id, isPen, samples)) return 0;
      const Sample& last = samples.back();
      g.hudType = isPen ? L"qalam" : (type == PT_MOUSE ? L"sichqoncha" : L"barmoq");
      g.hudPressure = last.p;
      if (ToDip(last.y) < kToolbarH) {  // panel: rang yoki o'lcham tanlash
        HandleToolbarClick(ToDip(last.x), ToDip(last.y));
        Render();
        return 0;
      }
      g.activeId = id;
      g.cur = Stroke{kColors[g.colorIdx], kSizes[g.sizeIdx], g.usePressure && isPen, {}};
      AppendSamples(samples, isPen);
      CountRate(samples.size());
      Render();
      UpdateLatency(last.t);
      return 0;
    }
    case WM_POINTERUPDATE: {
      if (!CollectSamples(id, isPen, samples)) return 0;
      const Sample& last = samples.back();
      g.hudType = isPen ? L"qalam" : (type == PT_MOUSE ? L"sichqoncha" : L"barmoq");
      g.hudPressure = isPen && !IS_POINTER_INCONTACT_WPARAM(wp) ? 0.f : last.p;
      if (id == g.activeId) {
        AppendSamples(samples, isPen);
        CountRate(samples.size());
        Render();  // darhol chizamiz: WM_PAINT kutilmaydi
        UpdateLatency(last.t);
      }
      return 0;
    }
    case WM_POINTERUP: {
      if (id == g.activeId) {
        if (CollectSamples(id, isPen, samples)) AppendSamples(samples, isPen);
        FinishStroke();
        Render();
      }
      return 0;
    }
    case WM_POINTERCAPTURECHANGED:
    case WM_POINTERLEAVE:
      if (id == g.activeId && msg == WM_POINTERCAPTURECHANGED) { FinishStroke(); Render(); }
      return 0;
  }
  return 0;
}

void Undo() {
  if (g.activeId || g.strokes.empty()) return;
  g.strokes.pop_back();
  RebuildDry();
  Render();
}

void Clear() {
  if (g.activeId) return;
  g.strokes.clear();
  RebuildDry();
  Render();
}

void ConfigurePenFeedback(HWND hwnd) {
  // Qalam bosib turganda chiqadigan halqa va kechiktiradigan imo-ishoralarni o'chiramiz.
  const BOOL off = FALSE;
  const FEEDBACK_TYPE types[] = {FEEDBACK_PEN_BARRELVISUALIZATION, FEEDBACK_PEN_TAP, FEEDBACK_PEN_DOUBLETAP, FEEDBACK_PEN_PRESSANDHOLD, FEEDBACK_PEN_RIGHTTAP};
  for (FEEDBACK_TYPE ft : types) SetWindowFeedbackSetting(hwnd, ft, 0, sizeof(off), &off);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      g.hwnd = hwnd;
      g.dpi = GetDpiForWindow(hwnd);
      ConfigurePenFeedback(hwnd);
      return 0;
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    case WM_POINTERLEAVE:
    case WM_POINTERCAPTURECHANGED:
      // DefWindowProc chaqirilmaydi: tizim qalamni sichqonchaga aylantirib, qo'shimcha kechikish qo'shmasin.
      return HandlePointer(msg, wp);
    case WM_SIZE:
      if (g.rt && wp != SIZE_MINIMIZED) {
        g.rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        SafeRelease(g.dryBrush);
        SafeRelease(g.dry);
        if (SUCCEEDED(g.rt->CreateCompatibleRenderTarget(&g.dry))) g.dry->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &g.dryBrush);
        RebuildDry();
      }
      return 0;
    case WM_DPICHANGED: {
      g.dpi = HIWORD(wp);
      const RECT* r = reinterpret_cast<const RECT*>(lp);
      SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
      DiscardTargets();
      return 0;
    }
    case WM_KEYDOWN: {
      const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      switch (wp) {
        case '1': g.colorIdx = 0; break;
        case '2': g.colorIdx = 1; break;
        case '3': g.colorIdx = 2; break;
        case 'Q': g.sizeIdx = 0; break;
        case 'W': g.sizeIdx = 1; break;
        case 'E': g.sizeIdx = 2; break;
        case 'P': g.usePressure = !g.usePressure; break;
        case 'C': Clear(); return 0;
        case 'Z': if (ctrl) { Undo(); return 0; } break;
        case VK_ESCAPE: DestroyWindow(hwnd); return 0;
      }
      Render();
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;  // fonni Direct2D chizadi (miltillashsiz)
    case WM_PAINT:
      Render();
      return 0;
    case WM_DESTROY:
      DiscardTargets();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  EnableMouseInPointer(TRUE);  // sichqoncha ham WM_POINTER bo'lib keladi: qalamsiz ham sinash mumkin
  QueryPerformanceFrequency(&g.freq);

  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g.factory))) return 1;
  g.factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), nullptr, 0, &g.roundStyle);
  if (SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&g.dwrite)))) {
    g.dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"", &g.hudFmt);
    if (g.hudFmt) g.hudFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
  wc.lpszClassName = L"JammHostProto";
  RegisterClassExW(&wc);

  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Jamm Host (prototip)", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, nullptr, nullptr, hInst, nullptr);
  if (!hwnd) return 1;
  ShowWindow(hwnd, nShow);
  UpdateWindow(hwnd);

  MSG m;
  while (GetMessage(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessage(&m);
  }
  SafeRelease(g.roundStyle);
  SafeRelease(g.hudFmt);
  SafeRelease(g.dwrite);
  SafeRelease(g.factory);
  return 0;
}
