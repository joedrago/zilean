#include "targetver.h"
#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "png.h"

#define RGBA(r,g,b,a) ((unsigned int)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)|(((DWORD)(BYTE)(a))<<24)))

typedef struct Pixel
{
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
} Pixel;

void pixelSet(Pixel *pixel, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    pixel->r = r;
    pixel->g = g;
    pixel->b = b;
    pixel->a = a;
}

int pixelMatches(Pixel *pixel, unsigned char r, unsigned char g, unsigned char b, unsigned char tolerance)
{
    if(pixel->r > (r + tolerance)) return 0;
    if(pixel->r < (r - tolerance)) return 0;
    if(pixel->g > (g + tolerance)) return 0;
    if(pixel->g < (g - tolerance)) return 0;
    if(pixel->b > (b + tolerance)) return 0;
    if(pixel->b < (b - tolerance)) return 0;
    return 1;
}

// ------------------------------------------------------------------------------------------------
// Generic helpers

void inflateRect(RECT *r, int i, int w, int h)
{
    r->left   -= i;
    r->top    -= i;
    r->right  += i;
    r->bottom += i;
    if(r->left < 0)   r->left = 0;
    if(r->top < 0)    r->top = 0;
    if(r->right > w)  r->right = w;
    if(r->bottom > h) r->bottom = h;
}

int closeEnough(int a, int b, int epsilon)
{
    int diff = a - b;
    if(diff < 0)
    {
        diff *= -1;
    }
    if(diff <= epsilon)
    {
        return 1;
    }
    return 0;
}

// ------------------------------------------------------------------------------------------------

typedef struct zBitmap
{
    int w;
    int h;
    Pixel *pixels;
} zBitmap;

zBitmap * zBitmapCreate(int w, int h)
{
    zBitmap * bmp = calloc(1, sizeof(zBitmap));
    bmp->w = w;
    bmp->h = h;
    bmp->pixels = VirtualAlloc(NULL, bmp->w * bmp->h * sizeof(unsigned int), MEM_COMMIT, PAGE_READWRITE);
    return bmp;
}

void zBitmapDestroy(zBitmap * bmp)
{
    VirtualFree(bmp->pixels, bmp->w * bmp->h * sizeof(unsigned int), MEM_RELEASE);
    free(bmp);
}

void zBitmapBox(zBitmap * zbmp, RECT *rect, int r, int g, int b)
{
    int i, j;
    if((rect->left < rect->right) && (rect->top < rect->bottom))
    {
        for (j = rect->top; j < rect->bottom; ++j)
        {
            Pixel * pixel = &zbmp->pixels[rect->left + (j * zbmp->w)];
            pixel->r = r;
            pixel->g = g;
            pixel->b = b;
            pixel = &zbmp->pixels[(rect->right-1) + (j * zbmp->w)];
            pixel->r = r;
            pixel->g = g;
            pixel->b = b;
        }
        for (i = rect->left; i < rect->right; ++i)
        {
            Pixel * pixel = &zbmp->pixels[i + (rect->top * zbmp->w)];
            pixel->r = r;
            pixel->g = g;
            pixel->b = b;
            pixel = &zbmp->pixels[i + ((rect->bottom-1) * zbmp->w)];
            pixel->r = r;
            pixel->g = g;
            pixel->b = b;
        }
    }
}

void zBitmapFill(zBitmap * zbmp, RECT *rect, int r, int g, int b)
{
    int i, j;
    if((rect->left < rect->right) && (rect->top < rect->bottom))
    {
        for (j = rect->top; j < rect->bottom; ++j)
        {
            for (i = rect->left; i < rect->right; ++i)
            {
                Pixel * pixel = &zbmp->pixels[i + (j * zbmp->w)];
                pixel->r = r;
                pixel->g = g;
                pixel->b = b;
            }
        }
    }
}

void zBitmapGrayscale(zBitmap * zbmp, RECT *rect)
{
    int i, j;
    if((rect->left < rect->right) && (rect->top < rect->bottom))
    {
        for (j = rect->top; j < rect->bottom; ++j)
        {
            for (i = rect->left; i < rect->right; ++i)
            {
                Pixel * pixel = &zbmp->pixels[i + (j * zbmp->w)];
                int sum = (int)pixel->r + (int)pixel->g + (int)pixel->b;
                int avg = sum / 3;
                pixel->r = avg;
                pixel->g = avg;
                pixel->b = avg;
            }
        }
    }
}

// alpha channel is the tolerance for that color
int zBitmapFindColoredBox(zBitmap *zbmp, RECT *subRect, int colorCount, Pixel *colors, int lineTolerance, RECT *outputRect)
{
    int i, j, k;
    int *colCounts;
    int *rowCounts;
    int bestRowCount = 0;
    int bestColCount = 0;

    RECT sub;
    if(subRect)
    {
        memcpy(&sub, subRect, sizeof(RECT));
    }
    else
    {
        sub.left = 0;
        sub.top = 0;
        sub.right = zbmp->w;
        sub.bottom = zbmp->h;
    }

    colCounts = (int *)calloc(sizeof(int), zbmp->w);
    rowCounts = (int *)calloc(sizeof(int), zbmp->h);

    for (j = sub.top; j < sub.bottom; ++j)
    {
        for (i = sub.left; i < sub.right; ++i)
        {
            int matches = 0;
            Pixel * pixel = &zbmp->pixels[i + (j * zbmp->w)];
            for(k = 0; k < colorCount; ++k)
            {
                if(pixelMatches(pixel, colors[k].r, colors[k].g, colors[k].b, colors[k].a))
                {
                    matches = 1;
                    break;
                }
            }
            if(matches)
            {
                ++colCounts[i];
                ++rowCounts[j];
            }
        }
    }

    for (i = sub.left; i < sub.right; ++i)
    {
        if(bestColCount < colCounts[i])
        {
            bestColCount = colCounts[i];
        }
    }
    for (j = sub.top; j < sub.bottom; ++j)
    {
        if(bestRowCount < rowCounts[j])
        {
            bestRowCount = rowCounts[j];
        }
    }

    outputRect->left = sub.right;
    outputRect->right = sub.left;
    for (i = sub.left; i < sub.right; ++i)
    {
        if((outputRect->left > i)
        && (closeEnough(colCounts[i], bestColCount, lineTolerance)))
        {
            outputRect->left = i;
        }
        if((outputRect->right < i)
        && (closeEnough(colCounts[i], bestColCount, lineTolerance)))
        {
            outputRect->right = i;
        }
    }
    outputRect->top = sub.bottom;
    outputRect->bottom = sub.top;
    for (j = sub.top; j < sub.bottom; ++j)
    {
        if((outputRect->top > j)
        && (closeEnough(rowCounts[j], bestRowCount, lineTolerance)))
        {
            outputRect->top = j;
        }
        if((outputRect->bottom < j)
        && (closeEnough(rowCounts[j], bestRowCount, lineTolerance)))
        {
            outputRect->bottom = j;
        }
    }

    free(colCounts);
    free(rowCounts);
    return 1;
}

// ------------------------------------------------------------------------------------------------

static zBitmap * captureScoreboard(HWND mainDlg)
{
    zBitmap * zbmp = NULL;
    //HWND captureWindow = FindWindow("RiotWindowClass", "League of Legends (TM) Client");
    HWND captureWindow = FindWindow("Notepad", NULL);
    if (captureWindow)
    {
        BITMAPINFOHEADER bi;
        HDC dc = GetDC(captureWindow);
        HDC bmpDC = CreateCompatibleDC(dc);
        HBITMAP bmp;
        HBITMAP oldBmp;
        RECT r;
        int width;
        int height;
        int lines;

        GetClientRect(captureWindow, &r);
        width = r.right;
        height = r.bottom;
        bmp = CreateCompatibleBitmap(dc, width, height);
        oldBmp = SelectObject(bmpDC, bmp);
        BitBlt(bmpDC, 0, 0, width, height, dc, 0, 0, SRCCOPY);

        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = width;
        bi.biHeight = -height;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;
        zbmp = zBitmapCreate(width, height);
        lines = GetDIBits(bmpDC, bmp, 0, height, zbmp->pixels, (BITMAPINFO *)&bi, DIB_RGB_COLORS);

        SelectObject(bmpDC, oldBmp);
        DeleteDC(bmpDC);
        ReleaseDC(captureWindow, dc);
        DeleteObject(bmp);

#if 0
        dc = GetDC(mainDlg);
        StretchDIBits(
            dc,
            0,
            0,
            200,
            200,
            0,
            0,
            width/2,
            height/2,
            zbmp->pixels,
            (BITMAPINFO *)&bi,
            DIB_RGB_COLORS,
            SRCCOPY
        );
        ReleaseDC(mainDlg, dc);
#endif
    }
    return zbmp;
}

zBitmap * loadScoreboard(const char * file_name);

static void checkScoreboard(HWND mainDlg)
{
#if 1
#else
    int scoreboardKeyHeld = (GetAsyncKeyState(VK_TAB) & 0x8000) ? 1 : 0;
    if (scoreboardKeyHeld)
    {
        zBitmap * zbmp = captureScoreboard(mainDlg);
        if (zbmp)
        {
            printf("got %dx%d pixels!\n", zbmp->w, zbmp->h);
            printf("First pixel color: 0x%x\n", zbmp->pixels[0]);
            zBitmapDestroy(zbmp);
        }
        else
        {
            printf("No game window found.\n");
        }
    }
    else
    {
        SetWindowText(GetDlgItem(mainDlg, IDC_INFO), "not held");
    }
#endif
}

zBitmap * loadScoreboard(const char * file_name)
{
    zBitmap *zbmp = NULL;
    png_byte header[8];
    png_structp png_ptr;
    png_infop info_ptr;
    png_infop end_info;
    int bit_depth, color_type;
    int temp_width, temp_height;
    int rowbytes;
    png_byte * image_data;
    png_byte ** row_pointers;
    int i;
    int j;

    FILE * fp = fopen(file_name, "rb");
    if (fp == 0)
    {
        perror(file_name);
        return 0;
    }

    // read the header
    fread(header, 1, 8, fp);

    if (png_sig_cmp(header, 0, 8))
    {
        fprintf(stderr, "error: %s is not a PNG.\n", file_name);
        fclose(fp);
        return 0;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf(stderr, "error: png_create_read_struct returned 0.\n");
        fclose(fp);
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        fclose(fp);
        return 0;
    }

    end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
        fclose(fp);
        return 0;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    // get info about png
    png_get_IHDR(png_ptr, info_ptr, &temp_width, &temp_height, &bit_depth, &color_type,
                 NULL, NULL, NULL);

    if (bit_depth != 8)
    {
        fprintf(stderr, "%s: Unsupported bit depth %d.  Must be 8.\n", file_name, bit_depth);
        return 0;
    }

    // Update the png info struct.
    png_read_update_info(png_ptr, info_ptr);

    // Row size in bytes.
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    // glTexImage2d requires rows to be 4-byte aligned
    rowbytes += 3 - ((rowbytes-1) % 4);

    // Allocate the image_data as a big block, to be given to opengl
    image_data = (png_byte *)malloc(rowbytes * temp_height * sizeof(png_byte)+15);
    if (image_data == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG image data\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return 0;
    }

    // row_pointers is for pointing to image_data for reading the png with libpng
    row_pointers = (png_byte **)malloc(temp_height * sizeof(png_byte *));
    if (row_pointers == NULL)
    {
        fprintf(stderr, "error: could not allocate memory for PNG row pointers\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        free(image_data);
        fclose(fp);
        return 0;
    }

    // set the individual row_pointers to point at the correct offsets of image_data
    for (i = 0; i < temp_height; i++)
    {
        row_pointers[temp_height - 1 - i] = image_data + i * rowbytes;
    }

    // read the png into image_data through row_pointers
    png_read_image(png_ptr, row_pointers);

    zbmp = zBitmapCreate(temp_width, temp_height);
    for(j = 0; j < temp_height; ++j)
    {
        for(i = 0; i < temp_width; ++i)
        {
            unsigned char *src = &image_data[(i * (rowbytes / temp_width)) + ((temp_height - (j+1)) * rowbytes)];
            Pixel *dst = &zbmp->pixels[i + (j * temp_width)];
            dst->r = src[0];
            dst->g = src[1];
            dst->b = src[2];
        }
    }

    // clean up
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    free(image_data);
    free(row_pointers);
    fclose(fp);
    return zbmp;
}

void findThings(zBitmap *zbmp)
{
    RECT subBox;
    RECT scoreBox;
    Pixel scoreColors[2];
    RECT facesBox;
    Pixel facesColors[4];

    // Rough greenish colors of outer scoreboard box
    pixelSet(&scoreColors[0], 24, 63, 60, 4);
    pixelSet(&scoreColors[1], 33, 69, 61, 4);
    zBitmapFindColoredBox(zbmp, NULL, 2, scoreColors, 30, &scoreBox);
    printf("scorebox location: [%d, %d, %d, %d]\n", scoreBox.left, scoreBox.top, scoreBox.right, scoreBox.bottom);
    //zBitmapFill(zbmp, &scoreBox, 255, 255, 0);

    // Grays that match the borders of the champion's faces
    pixelSet(&facesColors[0], 29, 30, 32, 1);
    pixelSet(&facesColors[1], 69, 69, 67, 1);
    pixelSet(&facesColors[2], 74, 75, 74, 1);
    pixelSet(&facesColors[3], 46, 47, 48, 1);
    memcpy(&subBox, &scoreBox, sizeof(RECT));
    subBox.right = subBox.left + ((subBox.right - subBox.left) / 8); // facesBox will be in the first 1/8th
    zBitmapFindColoredBox(zbmp, &subBox, 4, facesColors, 5, &facesBox);
    facesBox.right =  900;
    printf("facesbox location: [%d, %d, %d, %d]\n", facesBox.left, facesBox.top, facesBox.right, facesBox.bottom);
    zBitmapBox(zbmp, &facesBox, 255, 0, 255);
}

void debug(HWND mainDlg)
{
    zBitmap *zbmp = loadScoreboard("images\\board1.png");
    if(zbmp)
    {
        HDC dc = GetDC(mainDlg);
        BITMAPINFOHEADER bi;
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = zbmp->w;
        bi.biHeight = -zbmp->h;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;

        findThings(zbmp);

        StretchDIBits(
                dc,
                0,
                0,
                zbmp->w,
                zbmp->h,
                0,
                0,
                zbmp->w,
                zbmp->h,
                zbmp->pixels,
                (BITMAPINFO *)&bi,
                DIB_RGB_COLORS,
                SRCCOPY
                );
        ReleaseDC(mainDlg, dc);
        zBitmapDestroy(zbmp);
    }
}

// ------------------------------------------------------------------------------------------------

INT_PTR CALLBACK DlgProc(HWND mainDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static int DERP = 0;

    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
            SetTimer(mainDlg, 5, 500, NULL);
            return (INT_PTR)TRUE;

        case WM_TIMER:
            //checkScoreboard(mainDlg);
            if(!DERP)
            {
                debug(mainDlg);
                DERP = 1;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(mainDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

// ------------------------------------------------------------------------------------------------

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
    AllocConsole();
    SetConsoleTitle("Zilean Debug Output");
    freopen("CONOUT$", "w", stderr);
#endif

    {
//        zBitmap *zbmp = loadScoreboard("images\\board1.png");
//        return 0;
    }

    //    MSG msg;
    //    while (GetMessage(&msg, NULL, 0, 0))
    //    {
    //        if (!TranslateAccelerator(msg.mainDlg, hAccelTable, &msg))
    //        {
    //            TranslateMessage(&msg);
    //            DispatchMessage(&msg);
    //        }
    //    }
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_ZILEAN), NULL, DlgProc);
    return 0;
}
