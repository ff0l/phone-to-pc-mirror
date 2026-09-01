#include "icon.h"

#include "tokens.h"

#include <d2d1.h>
#include <dwrite_3.h>
#include <string.h>
#include <strsafe.h>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

static const UINT32 kXmark = 0xF00D;
static const int kIconSize = tokens::close_icon;

static HBITMAP g_xmark = NULL;
static HBITMAP g_xmark_on = NULL;

static bool read_file(const wchar_t *path, std::vector<unsigned char> *out) {
    HANDLE file;
    DWORD n;
    LARGE_INTEGER size;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 8 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    out->resize((size_t)size.QuadPart);
    if (!ReadFile(file, out->data(), (DWORD)size.QuadPart, &n, NULL) || n != (DWORD)size.QuadPart) {
        CloseHandle(file);
        out->clear();
        return false;
    }
    CloseHandle(file);
    return true;
}

static bool find_font(wchar_t *path, size_t path_cch) {
    wchar_t exe[MAX_PATH];
    wchar_t dir[MAX_PATH];
    wchar_t *slash;
    const wchar_t *names[] = {
        L"fa-solid-900.woff2",
        L"assets\\fa-solid-900.woff2",
        L"..\\assets\\fa-solid-900.woff2",
        L"C:\\repos\\etc\\icon library\\icons\\ui\\v7.3.0\\webfonts\\fa-solid-900.woff2",
        NULL
    };
    int i;
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wcsncpy(dir, exe, MAX_PATH);
    dir[MAX_PATH - 1] = 0;
    slash = wcsrchr(dir, L'\\');
    if (slash) {
        *slash = 0;
    }
    for (i = 0; names[i]; i++) {
        if (wcschr(names[i], L':')) {
            StringCchCopyW(path, path_cch, names[i]);
        } else {
            StringCchPrintfW(path, path_cch, L"%s\\%s", dir, names[i]);
        }
        if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }
    return false;
}

static HBITMAP raster_xmark(COLORREF color) {
    wchar_t font_path[MAX_PATH];
    std::vector<unsigned char> bytes;
    ComPtr<IDWriteFactory5> write;
    ComPtr<IDWriteFontFileStream> unpacked;
    ComPtr<IDWriteFontFile> file;
    ComPtr<IDWriteFontFace> face;
    ComPtr<ID2D1Factory> draw;
    ComPtr<IWICImagingFactory> imaging;
    ComPtr<IWICBitmap> bitmap;
    ComPtr<ID2D1RenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> brush;
    DWRITE_CONTAINER_TYPE kind;
    DWRITE_FONT_METRICS metrics;
    DWRITE_GLYPH_METRICS box;
    DWRITE_GLYPH_RUN run;
    DWRITE_GLYPH_OFFSET offset;
    DWRITE_FONT_FILE_TYPE file_type;
    DWRITE_FONT_FACE_TYPE face_type;
    D2D1_RENDER_TARGET_PROPERTIES props;
    HRESULT status;
    UINT16 glyph;
    UINT32 faces_on_file;
    BOOL supported;
    float em;
    float units;
    float ink_w;
    float ink_h;
    float origin_x;
    float origin_y;
    float top_from_base;
    float advance;
    wchar_t temp_dir[MAX_PATH];
    wchar_t temp_file[MAX_PATH];
    HANDLE disk;
    DWORD wrote;
    WICRect rect;
    UINT stride;
    std::vector<unsigned char> pixels;
    BITMAPINFO bmi;
    void *bits;
    HBITMAP dib;
    IDWriteFontFile *files[1];

    if (!find_font(font_path, MAX_PATH) || !read_file(font_path, &bytes)) {
        return NULL;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5), &write))) {
        return NULL;
    }
    kind = write->AnalyzeContainerType(bytes.data(), (UINT32)bytes.size());
    status = E_FAIL;
    if (kind == DWRITE_CONTAINER_TYPE_WOFF2 || kind == DWRITE_CONTAINER_TYPE_WOFF) {
        status = write->UnpackFontFile(kind, bytes.data(), (UINT32)bytes.size(), &unpacked);
    }
    if (SUCCEEDED(status) && unpacked) {
        UINT64 size = 0;
        const void *piece = NULL;
        void *cookie = NULL;
        if (FAILED(unpacked->GetFileSize(&size)) || size == 0 || size > 16ull * 1024ull * 1024ull) {
            return NULL;
        }
        if (FAILED(unpacked->ReadFileFragment(&piece, 0, size, &cookie))) {
            return NULL;
        }
        bytes.assign((const unsigned char *)piece, (const unsigned char *)piece + (size_t)size);
        unpacked->ReleaseFileFragment(cookie);
    }
    GetTempPathW(MAX_PATH, temp_dir);
    GetTempFileNameW(temp_dir, L"icn", 0, temp_file);
    disk = CreateFileW(temp_file, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (disk == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    WriteFile(disk, bytes.data(), (DWORD)bytes.size(), &wrote, NULL);
    CloseHandle(disk);
    if (FAILED(write->CreateFontFileReference(temp_file, NULL, &file)) || !file) {
        DeleteFileW(temp_file);
        return NULL;
    }
    if (FAILED(file->Analyze(&supported, &file_type, &face_type, &faces_on_file)) || !supported || faces_on_file == 0) {
        DeleteFileW(temp_file);
        return NULL;
    }
    files[0] = file.Get();
    if (FAILED(write->CreateFontFace(face_type, 1, files, 0, DWRITE_FONT_SIMULATIONS_NONE, &face))) {
        DeleteFileW(temp_file);
        return NULL;
    }
    DeleteFileW(temp_file);
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&draw)))) {
        return NULL;
    }
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&imaging)))) {
        return NULL;
    }
    if (FAILED(imaging->CreateBitmap(kIconSize, kIconSize, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &bitmap))) {
        return NULL;
    }
    props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                         D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(draw->CreateWicBitmapRenderTarget(bitmap.Get(), props, &target))) {
        return NULL;
    }
    target->CreateSolidColorBrush(D2D1::ColorF((float)GetRValue(color) / 255.0f, (float)GetGValue(color) / 255.0f,
                                               (float)GetBValue(color) / 255.0f, 1.0f),
                                  &brush);
    glyph = 0;
    face->GetGlyphIndices(&kXmark, 1, &glyph);
    if (glyph == 0) {
        return NULL;
    }
    face->GetMetrics(&metrics);
    face->GetDesignGlyphMetrics(&glyph, 1, &box, FALSE);
    em = (float)kIconSize * 0.92f;
    units = em / (float)metrics.designUnitsPerEm;
    ink_w = (float)(box.advanceWidth - box.leftSideBearing - box.rightSideBearing) * units;
    ink_h = (float)(box.advanceHeight - box.topSideBearing - box.bottomSideBearing) * units;
    if (ink_w < 1.0f) {
        ink_w = em * 0.75f;
    }
    if (ink_h < 1.0f) {
        ink_h = em * 0.75f;
    }
    origin_x = ((float)kIconSize - ink_w) * 0.5f - (float)box.leftSideBearing * units;
    top_from_base = -((float)box.verticalOriginY - (float)box.topSideBearing) * units;
    origin_y = ((float)kIconSize - ink_h) * 0.5f - top_from_base;
    ZeroMemory(&run, sizeof(run));
    run.fontFace = face.Get();
    run.fontEmSize = em;
    run.glyphCount = 1;
    run.glyphIndices = &glyph;
    advance = 0.0f;
    run.glyphAdvances = &advance;
    ZeroMemory(&offset, sizeof(offset));
    run.glyphOffsets = &offset;
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0, 0, 0));
    target->DrawGlyphRun(D2D1::Point2F(origin_x, origin_y), &run, brush.Get());
    if (FAILED(target->EndDraw())) {
        return NULL;
    }
    stride = (UINT)kIconSize * 4;
    pixels.resize((size_t)stride * kIconSize);
    rect.X = 0;
    rect.Y = 0;
    rect.Width = kIconSize;
    rect.Height = kIconSize;
    if (FAILED(bitmap->CopyPixels(&rect, stride, (UINT)pixels.size(), pixels.data()))) {
        return NULL;
    }
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kIconSize;
    bmi.bmiHeader.biHeight = -kIconSize;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib || !bits) {
        return NULL;
    }
    memcpy(bits, pixels.data(), pixels.size());
    return dib;
}

bool IconInit() {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (g_xmark && g_xmark_on) {
        return true;
    }
    if (!g_xmark) {
        g_xmark = raster_xmark(tokens::text_secondary);
    }
    if (!g_xmark_on) {
        g_xmark_on = raster_xmark(tokens::error);
    }
    return g_xmark != NULL && g_xmark_on != NULL;
}

void IconShutdown() {
    if (g_xmark) {
        DeleteObject(g_xmark);
        g_xmark = NULL;
    }
    if (g_xmark_on) {
        DeleteObject(g_xmark_on);
        g_xmark_on = NULL;
    }
}

HBITMAP IconXmark() {
    return g_xmark;
}

HBITMAP IconXmarkOn() {
    return g_xmark_on;
}
