// OptimizeKit app icon — 256x256 generator (PNG for the web UI/docs; .ico is produced from these PNGs)
// Drawn programmatically: dark rounded-square glass tile + rising performance gauge bars (3D) + bolt.
#include <windows.h>
#include <gdiplus.h>
#include <string>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static const CLSID kPng = { 0x557cf406, 0x1a04, 0x11d3, {0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e} };

static void savePng(Bitmap* b, const wchar_t* path) { b->Save(path, &kPng, nullptr); }

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    GdiplusStartupInput gsi;
    ULONG_PTR tok; GdiplusStartup(&tok, &gsi, nullptr);
    const int S = 256;
    Bitmap bmp(S, S, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // --- background: deep navy rounded tile with rim + glass sheen
    auto tile = Rect(10, 10, S - 20, S - 20);
    GraphicsPath pth; pth.AddArc(tile.X, tile.Y, 56, 56, 180, 90);
    pth.AddArc(tile.GetRight() - 56, tile.Y, 56, 56, 270, 90);
    pth.AddArc(tile.GetRight() - 56, tile.GetBottom() - 56, 56, 56, 0, 90);
    pth.AddArc(tile.X, tile.GetBottom() - 56, 56, 56, 90, 90); pth.CloseFigure();
    LinearGradientBrush bg(Point(0, 0), Point(0, S), Color(255, 16, 20, 32), Color(255, 34, 42, 64));
    g.FillPath(&bg, &pth);
    Pen rim(Color(200, 120, 150, 210), 3); g.DrawPath(&rim, &pth);
    LinearGradientBrush sheen(Point(0, 0), Point(0, S / 2), Color(70, 255, 255, 255), Color(0, 255, 255, 255));
    g.FillRectangle(&sheen, tile.X + 8, tile.Y + 6, tile.Width - 16, tile.Height / 2 - 10);

    // --- three rising bars (performance gauge) with 3D depth
    struct Bar { int x, h; Color c1, c2; };
    Bar bars[3] = {
        { 58,  66, Color(255,  90, 210, 160), Color(255,  38, 140,  96) },  // green
        { 106, 104, Color(255, 110, 170, 255), Color(255,  42, 100, 210) },  // blue
        { 154, 146, Color(255, 150, 200, 255), Color(255,  62, 120, 255) },  // bright blue
    };
    for (auto& b : bars) {
        int w = 34, y = 196 - b.h;
        SolidBrush side(Color(150, b.c2.GetR() / 2, b.c2.GetG() / 2, b.c2.GetB() / 2));
        g.FillRectangle(&side, b.x + w, y + 7, 10, b.h);
        LinearGradientBrush gb(Point(0, y), Point(0, y + b.h), b.c1, b.c2);
        g.FillRectangle(&gb, b.x, y, w, b.h);
        SolidBrush cap(Color(220, 255, 255, 255));
        g.FillRectangle(&cap, b.x, y, w, 4);
    }

    // --- lightning bolt (accent amber) with outline
    Point pts[] = { {150,28},{108,120},{138,120},{118,196},{196,96},{162,96},{186,28} };
    SolidBrush bolt(Color(245, 255, 196, 40));
    GraphicsPath bp; bp.AddPolygon(pts, 7);
    g.FillPath(&bolt, &bp);
    Pen bo(Color(235, 120, 80, 10), 3); g.DrawPath(&bo, &bp);

    savePng(&bmp, L"assets/icon256.png");
    Bitmap b48(48, 48, PixelFormat32bppARGB);
    { Graphics g2(&b48); g2.SetInterpolationMode(InterpolationModeHighQualityBicubic); g2.SetSmoothingMode(SmoothingModeAntiAlias); g2.DrawImage(&bmp, 0, 0, 48, 48); }
    savePng(&b48, L"assets/icon48.png");
    Bitmap b32(32, 32, PixelFormat32bppARGB);
    { Graphics g3(&b32); g3.SetInterpolationMode(InterpolationModeHighQualityBicubic); g3.SetSmoothingMode(SmoothingModeAntiAlias); g3.DrawImage(&bmp, 0, 0, 32, 32); }
    savePng(&b32, L"assets/icon32.png");

    GdiplusShutdown(tok);
    return 0;
}
