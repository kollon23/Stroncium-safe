#include <windows.h>
#define _USE_MATH_DEFINES 1
#include <cmath>
#include <vector>

struct SoundData {
    volatile bool running;
};

// Глобальная переменная для ответа квиза
int userSelectedAnswer = 0;

// Рандомный генератор randy()
static ULONGLONG n, r;
int randy() { return n = r, n ^= 0x8ebf635bee3c6d25, n ^= n << 5 | n >> 26, n *= 0xf3e05ca5c43e376b, r = n, n & 0x7fffffff; }

// Оконная процедура для кастомного квиза
LRESULT CALLBACK QuizWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        CreateWindowA("STATIC", "2+2?", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, 20, 240, 25, hwnd, NULL, NULL, NULL);
        CreateWindowA("BUTTON", "4", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 60, 100, 30, hwnd, (HMENU)1, NULL, NULL);
        CreateWindowA("BUTTON", "6", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 140, 60, 100, 30, hwnd, (HMENU)2, NULL, NULL);
        CreateWindowA("BUTTON", "22", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 20, 100, 100, 30, hwnd, (HMENU)3, NULL, NULL);
        CreateWindowA("BUTTON", "4719239", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 140, 100, 100, 30, hwnd, (HMENU)4, NULL, NULL);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id >= 1 && id <= 4) {
            userSelectedAnswer = id;
            DestroyWindow(hwnd);
        }
        break;
    }
    case WM_CLOSE:
        userSelectedAnswer = -1;
        DestroyWindow(hwnd);
        break;
    default:
        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void ShowQuizWindow() {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = QuizWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "QuizWindow";
    RegisterClassA(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int w = 280, h = 180;

    HWND hwnd = CreateWindowA("QuizWindow", "Quiz", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        (screenW - w) / 2, (screenH - h) / 2, w, h, NULL, NULL, wc.hInstance, NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!IsWindow(hwnd)) break;
    }
}

// ПОТОК ДЛЯ ИКОНКИ ОШИБКИ СО ШЛЕЙФОМ НА КУРСОРЕ
DWORD WINAPI iconPayload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    HICON hIcon = LoadIcon(NULL, IDI_ERROR);

    const int trailLength = 12;
    POINT trail[trailLength] = {0};
    int index = 0;

    POINT pt;
    GetCursorPos(&pt);
    for (int i = 0; i < trailLength; i++) {
        trail[i] = pt;
    }

    while (data->running) {
        HDC hdc = GetDC(NULL);
        GetCursorPos(&pt);
        
        trail[index] = pt;
        index = (index + 1) % trailLength;

        for (int i = 0; i < trailLength; i++) {
            int pIdx = (index + i) % trailLength;
            DrawIcon(hdc, trail[pIdx].x, trail[pIdx].y, hIcon);
        }

        ReleaseDC(NULL, hdc);
        Sleep(15);
    }
    return 0;
}

// ПОТОК: ДЕРГАНАЯ И АГРЕССИВНАЯ ТРЯСКА КУРССОРА
DWORD WINAPI cursorShakePayload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    while (data->running) {
        POINT pt;
        GetCursorPos(&pt);
        int offsetX = (rand() % 41) - 20; // от -20 до 20 пикселей
        int offsetY = (rand() % 41) - 20;
        SetCursorPos(pt.x + offsetX, pt.y + offsetY);
        Sleep(5); // Максимально резкий зуд
    }
    return 0;
}

// ВСПОМОГАТЕЛЬНЫЙ ШАРИК (для этапов)
DWORD WINAPI ellipsePayload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int signX = 1, signY = 1, incrementor = 10, x = 10, y = 10;
    DWORD startPayloadTime = GetTickCount();

    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        HDC hdc = GetDC(0);
        x += incrementor * signX;
        y += incrementor * signY;
        
        HBRUSH brush = CreateSolidBrush(RGB(rand() % 255, rand() % 255, rand() % 255));
        SelectObject(hdc, brush);
        Ellipse(hdc, x, y, 100 + x, 100 + y);

        if (y >= GetSystemMetrics(SM_CYSCREEN)) signY = -1;
        if (x >= GetSystemMetrics(SM_CXSCREEN)) signX = -1;
        if (y <= 0) signY = 1;
        if (x <= 0) signX = 1;

        Sleep(10);
        DeleteObject(brush);
        ReleaseDC(0, hdc);
    }
    return 0;
}

struct CustomRGB { union { COLORREF rgb; struct { BYTE b, g, r, unused; }; }; };

// АВТОНОМНЫЙ ЗВУКОВОЙ ПОТОК: все 12 этапов по 30 секунд
DWORD WINAPI SoundThread(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;

    // Этап 1
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? (10 * (t >> 6 | t | t >> (t >> 16)) + (7 & t >> 11)) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 2
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? (((t >> 10) | (t >> 4)) * t & ((t >> 8) | (t >> 10))) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 3
    {
        int sampleRate = 11025; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 330750) ? (t & (t >> 12)) * (t >> 4 | t >> 8) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 4
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? (t * (t ^ t + (t >> 15 | 1) ^ (t - 1280 ^ t) >> 10)) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 5
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? (2 * (t >> 5 & t) - (t >> 5) + t * (t >> 14 & 14)) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 6
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? (t * t * (1 + (t >> 8 & t >> 9)) & 128) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 7
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? (t>>6|t<<1)+(t>>5|t<<3|t>>3)|t>>2|t<<1 : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 8
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? (t + (t & t ^ t >> 6) - t * (t >> 9 & (t % 16 ? 2 : 6) & t >> 9)) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 9
    {
        int sampleRate = 11025; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            const char* melody = "kotojokohogohotojokohogokokhpmjmjfokhomkjhggcckotojokohogohotojokohogokokhpmjmjfrmorokfjkkkkkkrpomkjkmokmpoprrjjkkoommoottqqhjkkjjhfecebccccccfffhihfdfcdahhhjkjhghegcmjcjmjpjojmjkhchkhohmhkhjhkhmhohphmhoomkjkhhhhhhkotojokohogohotojokohogokokhpmjmjfokhomkjhggcckotojokohogohotojokohogokokhpmjmjfrmorokfjkkkkkkrpomkjkmokmpoprrjjkkoommoottqqhjkkjjhfecebccccccfffhihfdfcdahhhjkjhghegcmjcjmjpjojmjkhchkhohmhkhjhkhmhohphmhoomkjkhhhhhh";
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                int charIndex = (t >> 10 & 255);
                char c = melody[charIndex % strlen(melody)];
                long long val = (t < 330750) ? (static_cast<long long>(t * pow(2.0, (c - 58.0) / 12.0)) * 63 >> 6 & 132) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 10
    {
        int sampleRate = 32000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 960000) ? (t * (t << 1 ^ (t << 1) + (t >> 7) & (t >> 12) | (t >> 4) - (1 ^ 7 & (t >> 19)) | (t >> 7))) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // ЭТАП 11: Твоя возвращенная формула звука
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? 131072>t%262144?t/64>>3&2*t&10*t|t>>5&6*t&(t>>4|t>>5):131072<t%262144&163840>t%262144?t>>4&8*t&(t>>5|t>>4)|3*t&10*t:163840<t%262144&196608>t%262144?t>>4&8*t&(t>>5|t>>4)|3*t&6*t:196608<t%262144&229376>t%262144?t>>4&8*t&(t>>5|t>>4)|4*t&6*t:229376<t%262144&245760>t%262144?t>>4&8*t&(t>>5|t>>4)|4*t&2*t:t>>4&8*t&t>>4|(4*t&2*t)>>20 : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }
    if (!data->running) return 0;
    Sleep(300);

    // Этап 12
    {
        int sampleRate = 8000; int totalSamples = sampleRate * 30;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, (WORD)sampleRate, (DWORD)sampleRate, 1, 8, 0 };
        HWAVEOUT hWaveOut;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            std::vector<char> audioBuffer(totalSamples);
            for (int i = 0; i < totalSamples; i++) {
                unsigned long long t = i;
                long long val = (t < 240000) ? t*((t&4096?t%65536<59392?7:t&7:16)+(1&t>>14))>>(3&-t>>(t&2048?2:10))|t>>(t&16384?t&4096?10:3:2) : 0;
                audioBuffer[i] = static_cast<char>(val & 0xFF);
            }
            WAVEHDR header = { audioBuffer.data(), (DWORD)totalSamples, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
            DWORD startTime = GetTickCount();
            while (data->running && (GetTickCount() - startTime < 30000)) { if (header.dwFlags & WHDR_DONE) break; Sleep(50); }
            waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
        }
    }

    return 0;
}

// Пейлоад 2 этапа: пиксельный глитч
DWORD WINAPI payload31(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int ticks = GetTickCount(), w = GetSystemMetrics(0), h = GetSystemMetrics(1);
    RGBQUAD* bufferData = (RGBQUAD*)VirtualAlloc(0, (w * h + w) * sizeof(RGBQUAD), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    DWORD startPayloadTime = GetTickCount();
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        HDC hdc = GetDC(0), hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbm = CreateBitmap(w, h, 1, 32, bufferData);
        SelectObject(hdcMem, hbm);
        BitBlt(hdcMem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        GetBitmapBits(hbm, w * h * 4, bufferData);
        int v = 0; BYTE bt = 0;
        if ((GetTickCount() - ticks) > 60000) bt = rand() & 0xffffff;
        for (int j = 0; w * h > j; j++) {
            if (j % h == 0 && rand() % 100 == 0) v = rand() % 50;
            ((BYTE*)(bufferData + j))[v ? 252 : 252] += ((BYTE*)(bufferData + j))[j % 3] ^ bt;
        }
        SetBitmapBits(hbm, w * h * 4, bufferData);
        BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
        DeleteObject(hbm); DeleteObject(hdcMem);
        ReleaseDC(0, hdc);
        Sleep(10);
    }
    VirtualFree(bufferData, 0, MEM_RELEASE);
    return 0;
}

// ЭТАП 3: Зум-туннель + Шарик
DWORD WINAPI stage3Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int w = GetSystemMetrics(0), h = GetSystemMetrics(1);
    int signX = 1, signY = 1, incrementor = 10, x = 50, y = 50;
    DWORD startPayloadTime = GetTickCount();
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        HDC hdc = GetDC(0);
        StretchBlt(hdc, -20, -20, w + 40, h + 40, hdc, 0, 0, w, h, SRCCOPY);
        StretchBlt(hdc, 20, 20, w - 40, h - 40, hdc, 0, 0, w, h, SRCCOPY);
        x += incrementor * signX; y += incrementor * signY;
        HBRUSH brush = CreateSolidBrush(RGB(rand() % 255, rand() % 255, rand() % 255));
        SelectObject(hdc, brush);
        Ellipse(hdc, x, y, 100 + x, 100 + y);
        if (y >= h) signY = -1; if (x >= w) signX = -1; if (y <= 0) signY = 1; if (x <= 0) signX = 1;
        DeleteObject(brush);
        ReleaseDC(0, hdc);
        Sleep(0);
    }
    return 0;
}

// ЭТАП 4: Радужные Безье + Вертикальные волны
DWORD WINAPI stage4Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    DWORD startPayloadTime = GetTickCount();
    double angleSine = 0, angleColor = 0;
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        HDC hdc = GetDC(0);
        for (int k = 0; k < 4; k++) {
            POINT p[4] = {{0, 0}, {rand() % sw, rand() % sh}, {rand() % sw, rand() % sh}, {rand() % sw, rand() % sh}};
            int r = (int)(sin(angleColor) * 127 + 128);
            int g = (int)(sin(angleColor + 2.094) * 127 + 128);
            int b = (int)(sin(angleColor + 4.188) * 127 + 128);
            HPEN hPen = CreatePen(PS_SOLID, 3, RGB(r, g, b));
            SelectObject(hdc, hPen);
            PolyBezier(hdc, p, 4);
            DeleteObject(hPen);
            angleColor += 0.1;
        }
        for (int i = 0; i < sw; i++) {
            int a = (int)(sin(angleSine + i * 0.2) * 25);
            BitBlt(hdc, i, 0, 1, sh, hdc, i, a, SRCCOPY);
        }
        ReleaseDC(0, hdc);
        angleSine += M_PI / 2;
        Sleep(0);
    }
    return 0;
}

// ЭТАП 5: Пиксельный шейдер + Шарик
DWORD WINAPI stage5Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    BITMAPINFO bmi = { 40, sw, sh, 1, 24 };
    HANDLE hBall = CreateThread(NULL, 0, ellipsePayload, data, 0, NULL);
    DWORD startPayloadTime = GetTickCount();
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        HDC desk = GetDC(0), deskMem = CreateCompatibleDC(desk);
        PRGBTRIPLE rgbtriple;
        HBITMAP scr = CreateDIBSection(desk, &bmi, 0, (void**)&rgbtriple, 0, 0);
        SelectObject(deskMem, scr);
        BitBlt(deskMem, 0, 0, sw, sh, desk, 0, 0, SRCCOPY);
        for (int i = 0; i < sw * sh; i++) {
            rgbtriple[i].rgbtRed += GetRValue(i) & i;
            rgbtriple[i].rgbtGreen += GetGValue(i) & i;
            rgbtriple[i].rgbtBlue += GetBValue(i) & i;
        }
        BitBlt(desk, 0, 0, sw, sh, deskMem, 0, 0, SRCCOPY);
        ReleaseDC(NULL, desk);
        DeleteDC(deskMem); DeleteObject(scr); DeleteObject(rgbtriple);
        Sleep(10);
    }
    WaitForSingleObject(hBall, 1000);
    CloseHandle(hBall);
    return 0;
}

// ЭТАП 6: Код линий через память
DWORD WINAPI stage6Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    int yfull = GetSystemMetrics(SM_CYFULLSCREEN), xfull = GetSystemMetrics(SM_CXFULLSCREEN);
    int rainbow = w - rand() % w - (w / 150 - 112) % 149, inc = round(w / 100);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bmi.bmiHeader.biWidth = w; bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
    DWORD startPayloadTime = GetTickCount();
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        DWORD elapsed = GetTickCount() - startPayloadTime;
        int alpha = (elapsed < 3000) ? (int)((double)elapsed / 3000.0 * 255.0) : 255;
        HDC hdc = GetDC(0), hdcMem = CreateCompatibleDC(hdc);
        UINT* pixelPtr = nullptr;
        HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&pixelPtr, NULL, 0);
        if (!hbm || !pixelPtr) { DeleteDC(hdcMem); ReleaseDC(0, hdc); continue; }
        SelectObject(hdcMem, hbm);
        for (int yp = 0; yp < h; ++yp) {
            for (int xp = 0; xp < w; ++xp) {
                int xa = inc * xp;
                int x1 = (xp + yp) % w, y1 = yp;
                if (x1 >= 0 && x1 < w && y1 >= 0 && y1 < h) pixelPtr[y1 * w + x1] = RGB(xa, xa, xa);
                int x2 = yp, y2 = rainbow;
                if (x2 >= 0 && x2 < w && y2 >= 0 && y2 < h) pixelPtr[y2 * w + x2] = RGB(yfull, xa, xfull);
                int x3 = rainbow, y3 = rainbow;
                if (x3 >= 0 && x3 < w && y3 >= 0 && y3 < h) pixelPtr[y3 * w + x3] = RGB(rainbow, rainbow, rainbow);
                int x4 = rainbow, y4 = yp;
                if (x4 >= 0 && x4 < w && y4 >= 0 && y4 < h) pixelPtr[y4 * w + x4] = RGB(xfull, yfull, xfull);
            }
        }
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
        AlphaBlend(hdc, 0, 0, w, h, hdcMem, 0, 0, w, h, bf);
        DeleteObject(hbm); DeleteDC(hdcMem); ReleaseDC(0, hdc);
    }
    return 0;
}

// ЭТАП 7: Шейдер + Sleep(1) + Шарик
DWORD WINAPI stage7Payload(LPVOID lpParam) {
    SoundData* sndData = (SoundData*)lpParam;
    int time = GetTickCount(), w = GetSystemMetrics(0), h = GetSystemMetrics(1);
    RGBQUAD* pixelData = (RGBQUAD*)VirtualAlloc(0, (w * h + w) * sizeof(RGBQUAD), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    HANDLE hBall = CreateThread(NULL, 0, ellipsePayload, sndData, 0, NULL);
    DWORD startPayloadTime = GetTickCount();
    while (sndData->running && (GetTickCount() - startPayloadTime < 30000)) {
        HDC desk = GetDC(NULL), hdcdc = CreateCompatibleDC(desk);
        HBITMAP hbm = CreateBitmap(w, h, 1, 32, pixelData);
        SelectObject(hdcdc, hbm);
        BitBlt(hdcdc, 0, 0, w, h, desk, 0, 0, SRCCOPY);
        GetBitmapBits(hbm, 4 * h * w, pixelData);
        int v = 0; BYTE byte = 0;
        if ((GetTickCount() - time) > 10) byte = randy() % 0xff;
        for (int i = 0; w * h > i; ++i) {
            if (!(i % h) && !(randy() % 110)) v = randy() % 24;
            *((BYTE*)pixelData + 4 * i + v) -= 5;
        }
        SetBitmapBits(hbm, w * h * 4, pixelData);
        BitBlt(desk, 0, 0, w, h, hdcdc, 0, 0, SRCCOPY);
        DeleteObject(hbm); DeleteDC(hdcdc); ReleaseDC(NULL, desk);
        Sleep(1);
    }
    WaitForSingleObject(hBall, 1000); CloseHandle(hBall);
    VirtualFree(pixelData, 0, MEM_RELEASE);
    return 0;
}

// ЭТАП 8: Код иконок и автоочистки
DWORD WINAPI stage8Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int time = GetTickCount(), w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    RGBQUAD* pixelData = (RGBQUAD*)VirtualAlloc(0, (w * h + w) * sizeof(RGBQUAD), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LPCTSTR iconTypes[] = { IDI_ERROR, IDI_WARNING, IDI_INFORMATION, IDI_QUESTION, IDI_WINLOGO };
    int iconCount = sizeof(iconTypes) / sizeof(iconTypes[0]);
    DWORD startPayloadTime = GetTickCount(), lastClearTime = GetTickCount(), clearInterval = 200 + (rand() % 801);
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        DWORD currentTime = GetTickCount();
        if (currentTime - lastClearTime >= clearInterval) {
            InvalidateRect(NULL, NULL, TRUE);
            RedrawWindow(NULL, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
            lastClearTime = currentTime; clearInterval = 200 + (rand() % 801);
        }
        HDC desk = GetDC(NULL), hdcdc = CreateCompatibleDC(desk);
        HBITMAP hbm = CreateBitmap(w, h, 1, 32, pixelData);
        SelectObject(hdcdc, hbm);
        BitBlt(hdcdc, 0, 0, w, h, desk, 0, 0, SRCCOPY);
        GetBitmapBits(hbm, w * h * 4, pixelData);
        int v = 0; BYTE byte = 0;
        if ((GetTickCount() - time) > 60000) byte = randy() % 0xff;
        for (int i = 0; w * h > i; i++) {
            if (i % h == 0 && randy() % 100 == 0) v = randy() % 50;
            ((BYTE*)(pixelData + i))[v % 3] += ((BYTE*)(pixelData + i + v))[v] ^ byte;
        }
        SetBitmapBits(hbm, w * h * 4, pixelData);
        BitBlt(desk, 0, 0, w, h, hdcdc, 0, 0, SRCCOPY);
        int x = rand() % w, y = rand() % h, customWidth = 16 + (rand() % 113), customHeight = 16 + (rand() % 113);
        HICON hRandIcon = LoadIcon(NULL, iconTypes[rand() % iconCount]);
        DrawIconEx(desk, x, y, hRandIcon, customWidth, customHeight, 0, NULL, DI_NORMAL);
        DeleteObject(hbm); DeleteDC(hdcdc); ReleaseDC(NULL, desk);
        Sleep(1);
    }
    VirtualFree(pixelData, 0, MEM_RELEASE);
    return 0;
}

// ЭТАП 9: Вертикальный StretchBlt
DWORD WINAPI stage9Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    DWORD startPayloadTime = GetTickCount();
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        HDC desk = GetDC(0);
        int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
        StretchBlt(desk, 0, -20, sw, sh + 40, desk, 0, 0, sw, sh, SRCCOPY);
        ReleaseDC(0, desk);
        Sleep(4);
    }
    return 0;
}

inline void ciOptimized(HDC hdc, int x, int y, int w, int h) {
    HRGN hrgn = CreateEllipticRgn(x, y, w + x, h + y);
    SelectClipRgn(hdc, hrgn);
    BitBlt(hdc, x, y, w, h, hdc, x, y, NOTSRCCOPY);
    SelectClipRgn(hdc, NULL);
    DeleteObject(hrgn);
}

// ЭТАП 10: Оптимизированный
DWORD WINAPI stage10Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    int rw = w - 500, rh = h - 500;
    HDC hdc = GetDC(0), hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmMem);
    BitBlt(hdcMem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    DWORD startPayloadTime = GetTickCount();
    int redrawCounter_s2 = 0;
    while (data->running && (GetTickCount() - startPayloadTime < 30000)) {
        BitBlt(hdcMem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        const float amplitude = 25.0f;
        for (float t = 0.0f; t < 10.0f; t += 0.5f) {
            float sinValue = sinf(t);
            redrawCounter_s2 += 10;
            int py = redrawCounter_s2, px = (int)(sinValue * amplitude);
            if (redrawCounter_s2 >= h) redrawCounter_s2 = 0;
            BitBlt(hdc, px, py, w, 10, hdcMem, 0, py, SRCCOPY);
        }
        const int size = 1000;
        int cx = rand() % (rw + size) - size / 2, cy = rand() % (rh + size) - size / 2;
        for (int i = 0; i < size; i += 200) {
            ciOptimized(hdc, cx - i / 2, cy - i / 2, i, i);
            Sleep(5);
        }
    }
    SelectObject(hdcMem, hOldBmp); DeleteObject(hbmMem); DeleteDC(hdcMem); ReleaseDC(0, hdc);
    return 0;
}

// ЭТАП 11: Мерцающий шейдер + Летающая надпись "Stroncium.exe" (DVD-стиль)
DWORD WINAPI stage11Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int w = GetSystemMetrics(0), h = GetSystemMetrics(1);
    CustomRGB* pixels = (CustomRGB*)VirtualAlloc(0, (w * h + w) * sizeof(CustomRGB), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    int textW = 140, textH = 35;
    int px = rand() % (w - textW), py = rand() % (h - textH), vx = 4, vy = 4;
    double cAngle = 0;
    DWORD start = GetTickCount();
    
    while (data->running && (GetTickCount() - start < 30000)) {
        HDC desk = GetDC(NULL), memDC = CreateCompatibleDC(desk);
        HBITMAP hbm = CreateBitmap(w, h, 1, 32, pixels);
        SelectObject(memDC, hbm);
        BitBlt(memDC, 0, 0, w, h, desk, 0, 0, SRCCOPY);
        GetBitmapBits(hbm, w * h * 4, pixels);
        
        for (int i = 0; i < w * h; i++) {
            pixels[i].rgb = (pixels[i].rgb * 2) % RGB(255, 255, 255);
        }
        
        SetBitmapBits(hbm, w * h * 4, pixels);
        BitBlt(desk, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        px += vx; py += vy;
        if (px <= 0 || px + textW >= w) vx = -vx;
        if (py <= 0 || py + textH >= h) vy = -vy;

        int r = (int)(sin(cAngle) * 127 + 128), g = (int)(sin(cAngle + 2.09) * 127 + 128), b = (int)(sin(cAngle + 4.18) * 127 + 128);
        cAngle += 0.05;

        HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
        HPEN pen = CreatePen(PS_NULL, 0, 0);
        SelectObject(desk, brush); 
        SelectObject(desk, pen);
        Rectangle(desk, px, py, px + textW, py + textH);

        SetBkMode(desk, TRANSPARENT);
        SetTextColor(desk, RGB(255 - r, 255 - g, 255 - b));
        
        HFONT font = CreateFontA(15, 8, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, 0, 0, "Arial");
        SelectObject(desk, font);
        
        RECT rect = { px, py, px + textW, py + textH };
        DrawTextA(desk, "Stroncium.exe", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        DeleteObject(font); DeleteObject(brush); DeleteObject(pen); DeleteObject(hbm); DeleteObject(memDC); ReleaseDC(NULL, desk);
        Sleep(10);
    }
    VirtualFree(pixels, 0, MEM_RELEASE);
    return 0;
}

// ЭТАП 12: Мерцающий шейдер + отскакивающий шарик + Безье из левого верхнего угла {0,0}
DWORD WINAPI stage12Payload(LPVOID lpParam) {
    SoundData* data = (SoundData*)lpParam;
    int w = GetSystemMetrics(0), h = GetSystemMetrics(1);
    CustomRGB* pixels = (CustomRGB*)VirtualAlloc(0, (w * h + w) * sizeof(CustomRGB), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    int ballX = rand() % (w - 100), ballY = rand() % (h - 100);
    int ballVx = 8, ballVy = 8;
    int ballSize = 80;

    DWORD start = GetTickCount();
    while (data->running && (GetTickCount() - start < 30000)) {
        HDC desk = GetDC(NULL), memDC = CreateCompatibleDC(desk);
        HBITMAP hbm = CreateBitmap(w, h, 1, 32, pixels);
        SelectObject(memDC, hbm);
        BitBlt(memDC, 0, 0, w, h, desk, 0, 0, SRCCOPY);
        GetBitmapBits(hbm, w * h * 4, pixels);
        
        for (int i = 0; i < w * h; i++) {
            pixels[i].rgb = (pixels[i].rgb * 2) % RGB(255, 255, 255);
        }
        
        SetBitmapBits(hbm, w * h * 4, pixels);
        BitBlt(desk, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        ballX += ballVx;
        ballY += ballVy;
        if (ballX <= 0 || ballX + ballSize >= w) ballVx = -ballVx;
        if (ballY <= 0 || ballY + ballSize >= h) ballVy = -ballVy;

        HBRUSH ballBrush = CreateSolidBrush(RGB(rand() % 255, rand() % 255, rand() % 255));
        SelectObject(desk, ballBrush);
        Ellipse(desk, ballX, ballY, ballX + ballSize, ballY + ballSize);
        DeleteObject(ballBrush);

        HPEN bezierPen = CreatePen(PS_SOLID, 2, RGB(rand() % 255, rand() % 255, rand() % 255));
        SelectObject(desk, bezierPen);
        POINT pts[4] = {
            {0, 0},
            {rand() % w, rand() % h},
            {rand() % w, rand() % h},
            {rand() % w, rand() % h}
        };
        PolyBezier(desk, pts, 4);
        DeleteObject(bezierPen);

        DeleteObject(hbm);
        DeleteObject(memDC);
        ReleaseDC(NULL, desk);
        Sleep(10);
    }
    VirtualFree(pixels, 0, MEM_RELEASE);
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    // 1. Квиз (2+2)
    ShowQuizWindow();

    if (userSelectedAnswer != 1) {
        MessageBoxA(NULL, "Bro, do you have an IQ of -23?", "Bruh", MB_OK | MB_ICONERROR);
        return 0;
    }

    // 2. Первое предупреждение
    if (MessageBoxA(NULL, "Run Stroncium?", "Warning", MB_YESNO | MB_ICONWARNING) != IDYES) return 0;

    // 3. Второе предупреждение
    if (MessageBoxA(NULL, "Are you sure? This is GDI Only!", "Warning", MB_YESNO | MB_ICONWARNING) != IDYES) return 0;

    SoundData soundData = { true };
    
    // Запускаем фоновые потоки: звук, шлейф иконки и агрессивную тряску курсора
    HANDLE hThread = CreateThread(NULL, 0, SoundThread, &soundData, 0, NULL);
    HANDLE hIconThread = CreateThread(NULL, 0, iconPayload, &soundData, 0, NULL);
    HANDLE hShakeThread = CreateThread(NULL, 0, cursorShakePayload, &soundData, 0, NULL);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // ==========================================
    // ЭТАП 1: Синусоиды (30 сек)
    // ==========================================
    {
        DWORD startTime = GetTickCount();
        double angle = 0.0;
        while (soundData.running && (GetTickCount() - startTime < 30000)) {
            HDC hdc = GetDC(NULL);
            DWORD t = GetTickCount() - startTime;
            long long formulaVal = (t < 240000) ? (10 * (t >> 6 | t | t >> (t >> 16)) + (7 & t >> 11)) : 0;
            double dynamicFactor = static_cast<double>(formulaVal % 15 + 5);

            for (int y = 0; y < screenHeight; y += 10) {
                int shiftX = static_cast<int>(sin(angle + y * 0.05) * dynamicFactor);
                BitBlt(hdc, shiftX, y, screenWidth, 10, hdc, 0, y, SRCCOPY);
            }
            for (int x = 0; x < screenWidth; x += 10) {
                int shiftY = static_cast<int>(cos(angle + x * 0.05) * dynamicFactor);
                BitBlt(hdc, x, shiftY, 10, screenHeight, hdc, x, 0, SRCCOPY);
            }

            ReleaseDC(NULL, hdc);
            angle += 0.15;
            Sleep(1);
        }
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 2: Пиксели + Шарики (30 сек)
    // ==========================================
    {
        HANDLE hP1 = CreateThread(NULL, 0, payload31, &soundData, 0, NULL);
        HANDLE hP2 = CreateThread(NULL, 0, ellipsePayload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP1, 1000);
        WaitForSingleObject(hP2, 1000);
        CloseHandle(hP1);
        CloseHandle(hP2);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 3: Зум-туннель + Шарик (30 сек)
    // ==========================================
    {
        HANDLE hP3 = CreateThread(NULL, 0, stage3Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP3, 1000);
        CloseHandle(hP3);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 4: Радужные Безье + Вертикальные волны (30 сек)
    // ==========================================
    {
        HANDLE hP4 = CreateThread(NULL, 0, stage4Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP4, 1000);
        CloseHandle(hP4);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 5: Пиксельный шейдер + Шарик (30 сек)
    // ==========================================
    {
        HANDLE hP5 = CreateThread(NULL, 0, stage5Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP5, 1000);
        CloseHandle(hP5);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 6: Код линий (30 сек)
    // ==========================================
    {
        HANDLE hP6 = CreateThread(NULL, 0, stage6Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP6, 1000);
        CloseHandle(hP6);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 7: Шейдер randy + Шарик (30 сек)
    // ==========================================
    {
        HANDLE hP7 = CreateThread(NULL, 0, stage7Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP7, 1000);
        CloseHandle(hP7);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 8: Код иконок (30 сек)
    // ==========================================
    {
        HANDLE hP8 = CreateThread(NULL, 0, stage8Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP8, 1000);
        CloseHandle(hP8);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 9: Вертикальный StretchBlt (30 сек)
    // ==========================================
    {
        HANDLE hP9 = CreateThread(NULL, 0, stage9Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP9, 1000);
        CloseHandle(hP9);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 10: Эллипсы + Волны + Sleep(5) (30 сек)
    // ==========================================
    {
        HANDLE hP10 = CreateThread(NULL, 0, stage10Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP10, 1000);
        CloseHandle(hP10);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 11: Мерцающий шейдер + Летающая надпись (30 сек)
    // ==========================================
    {
        HANDLE hP11 = CreateThread(NULL, 0, stage11Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP11, 1000);
        CloseHandle(hP11);
    }

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);
    Sleep(300);

    // ==========================================
    // ЭТАП 12: Мерцающий шейдер + Шарик + Безье из {0,0} (30 сек)
    // ==========================================
    {
        HANDLE hP12 = CreateThread(NULL, 0, stage12Payload, &soundData, 0, NULL);
        Sleep(30000);
        WaitForSingleObject(hP12, 1000);
        CloseHandle(hP12);
    }

    // Завершение работы программы
    soundData.running = false;
    WaitForSingleObject(hThread, 1000);
    WaitForSingleObject(hIconThread, 1000);
    WaitForSingleObject(hShakeThread, 1000);
    CloseHandle(hThread);
    CloseHandle(hIconThread);
    CloseHandle(hShakeThread);

    InvalidateRect(NULL, NULL, TRUE);
    UpdateWindow(NULL);

    return 0;
}