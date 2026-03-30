// winui.c : interface Win32 pour le panneau avionique

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "pannel.h"
#include "calculateur.h"

#define MAX_SAMPLES 200

static int   g_altHistory[MAX_SAMPLES];
static int   g_sampleCount = 0;


// IDs pour les contrôles
#define IDC_ALT_INPUT   101
#define IDC_TAUX_INPUT  102
#define IDC_ANGLE_INPUT 103
#define IDC_BTN_SEND    104

#define IDC_ALT_VALUE   201
#define IDC_TAUX_VALUE  202
#define IDC_PUISS_VALUE 203
#define IDC_MODE_VALUE  204

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Etat global (simple pour une petite appli)
static Panel g_panel = NULL;
static HWND  g_hAltValue = NULL;
static HWND  g_hTauxValue = NULL;
static HWND  g_hPuissValue = NULL;
static HWND  g_hModeValue = NULL;
static HWND  g_hAltInput = NULL;
static HWND  g_hTauxInput = NULL;
static HWND  g_hAngleInput = NULL;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    g_panel = panel_init();
    if (!g_panel) return 1;

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "AvioniquePanelClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Echec RegisterClass", "Erreur", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowA(
        "AvioniquePanelClass",
        "Panneau avionique (Win32)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxA(NULL, "Echec CreateWindow", "Erreur", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    panel_destroy(&g_panel);
    return (int)msg.wParam;
}

static void update_display_from_panel(void)
{
    PanelDisplay d = panel_get_display(g_panel);
    char buf[64];

    snprintf(buf, sizeof(buf), "%d ft", d.altitude_ft);
    SetWindowTextA(g_hAltValue, buf);

    snprintf(buf, sizeof(buf), "%.1f m/min", d.vitesse_mpm);
    SetWindowTextA(g_hTauxValue, buf);

    snprintf(buf, sizeof(buf), "%.1f %%", d.puissance_pct);
    SetWindowTextA(g_hPuissValue, buf);

    const char *modeStr = "INCONNU";
    switch (d.mode) {
        case MODE_AU_SOL:         modeStr = "AU_SOL"; break;
        case MODE_CHANGEMENT_ALT: modeStr = "CHANGEMENT_ALT"; break;
        case MODE_VOL_CROISIERE:  modeStr = "VOL_CROISIERE"; break;
    }
    SetWindowTextA(g_hModeValue, modeStr);
}

static void handle_send(HWND hwnd)
{
    char buf[64];
    GetWindowTextA(g_hAltInput, buf, sizeof(buf));
    int alt = (int)strtol(buf, NULL, 10);

    GetWindowTextA(g_hTauxInput, buf, sizeof(buf));
    float taux = (float)strtod(buf, NULL);

    GetWindowTextA(g_hAngleInput, buf, sizeof(buf));
    float ang = (float)strtod(buf, NULL);

    if (!panel_set_altitude_desiree(g_panel, alt) ||
        !panel_set_taux_montee(g_panel, taux) ||
        !panel_set_angle(g_panel, ang)) {
        const char *err = panel_get_last_error(g_panel);
        MessageBoxA(hwnd, err ? err : "Erreur inconnue",
                    "Erreur panel", MB_ICONERROR);
        return;
    }

    // appeler le calculateur
    PanelInputs  in  = panel_get_inputs(g_panel);
    PanelDisplay st  = panel_get_display(g_panel);
    CalcInput    cin = { in, st };
    CalcOutput   cout;

    calculateur_run(&cin, &cout);
    panel_set_display(g_panel, &cout.state_out);

    update_display_from_panel();

    PanelDisplay d = panel_get_display(g_panel);

    /* décalage simple dans l'historique */
    if (g_sampleCount < MAX_SAMPLES) {
        g_altHistory[g_sampleCount++] = d.altitude_ft;
    } else {
        memmove(&g_altHistory[0], &g_altHistory[1],
                (MAX_SAMPLES - 1) * sizeof(int));
        g_altHistory[MAX_SAMPLES - 1] = d.altitude_ft;
    }

    /* redessiner la fenêtre (et donc le graphe) */
    InvalidateRect(hwnd, NULL, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE: {
        // Création des labels et champs
        CreateWindowA("STATIC", "Altitude actuelle :",
                      WS_VISIBLE | WS_CHILD,
                      20, 20, 150, 20,
                      hwnd, NULL, NULL, NULL);
        g_hAltValue = CreateWindowA("STATIC", "0 ft",
                      WS_VISIBLE | WS_CHILD | SS_LEFT,
                      200, 20, 150, 20,
                      hwnd, (HMENU)IDC_ALT_VALUE, NULL, NULL);

        CreateWindowA("STATIC", "Taux de montee :",
                      WS_VISIBLE | WS_CHILD,
                      20, 50, 150, 20,
                      hwnd, NULL, NULL, NULL);
        g_hTauxValue = CreateWindowA("STATIC", "0.0 m/min",
                      WS_VISIBLE | WS_CHILD | SS_LEFT,
                      200, 50, 150, 20,
                      hwnd, (HMENU)IDC_TAUX_VALUE, NULL, NULL);

        CreateWindowA("STATIC", "Puissance moteur :",
                      WS_VISIBLE | WS_CHILD,
                      20, 80, 150, 20,
                      hwnd, NULL, NULL, NULL);
        g_hPuissValue = CreateWindowA("STATIC", "0 %",
                      WS_VISIBLE | WS_CHILD | SS_LEFT,
                      200, 80, 150, 20,
                      hwnd, (HMENU)IDC_PUISS_VALUE, NULL, NULL);

        CreateWindowA("STATIC", "Mode :",
                      WS_VISIBLE | WS_CHILD,
                      20, 110, 150, 20,
                      hwnd, NULL, NULL, NULL);
        g_hModeValue = CreateWindowA("STATIC", "AU_SOL",
                      WS_VISIBLE | WS_CHILD | SS_LEFT,
                      200, 110, 150, 20,
                      hwnd, (HMENU)IDC_MODE_VALUE, NULL, NULL);

        // Entrées utilisateur
        CreateWindowA("STATIC", "Altitude desiree (ft) :",
                      WS_VISIBLE | WS_CHILD,
                      20, 160, 170, 20,
                      hwnd, NULL, NULL, NULL);
        g_hAltInput = CreateWindowA("EDIT", "",
                      WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                      200, 160, 100, 20,
                      hwnd, (HMENU)IDC_ALT_INPUT, NULL, NULL);

        CreateWindowA("STATIC", "Taux de montee (m/min) :",
                      WS_VISIBLE | WS_CHILD,
                      20, 190, 170, 20,
                      hwnd, NULL, NULL, NULL);
        g_hTauxInput = CreateWindowA("EDIT", "0",
                      WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                      200, 190, 100, 20,
                      hwnd, (HMENU)IDC_TAUX_INPUT, NULL, NULL);

        CreateWindowA("STATIC", "Angle d'attaque (deg) :",
                      WS_VISIBLE | WS_CHILD,
                      20, 220, 170, 20,
                      hwnd, NULL, NULL, NULL);
        g_hAngleInput = CreateWindowA("EDIT", "0",
                      WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                      200, 220, 100, 20,
                      hwnd, (HMENU)IDC_ANGLE_INPUT, NULL, NULL);

        // Bouton
        CreateWindowA("BUTTON", "Envoyer",
                      WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                      20, 260, 100, 30,
                      hwnd, (HMENU)IDC_BTN_SEND, NULL, NULL);

        update_display_from_panel();
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_SEND &&
            HIWORD(wParam) == BN_CLICKED) {
            handle_send(hwnd);

        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        // zone graphe (par ex. en bas de fenêtre)
        RECT plotRect = rc;
        plotRect.top = 320;    // à ajuster selon la taille de la fenêtre

        // fond blanc
        FillRect(hdc, &plotRect, (HBRUSH)(COLOR_WINDOW+1));

        // axes simples
        MoveToEx(hdc, plotRect.left + 40, plotRect.top + 10, NULL);
        LineTo(hdc, plotRect.left + 40, plotRect.bottom - 20);
        LineTo(hdc, plotRect.right - 10, plotRect.bottom - 20);

        if (g_sampleCount > 1) {
            // trouver min/max pour normaliser
            int minAlt = g_altHistory[0];
            int maxAlt = g_altHistory[0];
            for (int i = 1; i < g_sampleCount; ++i) {
                if (g_altHistory[i] < minAlt) minAlt = g_altHistory[i];
                if (g_altHistory[i] > maxAlt) maxAlt = g_altHistory[i];
            }
            if (maxAlt == minAlt) maxAlt = minAlt + 1; // éviter division par 0

            int width  = (plotRect.right - 60);
            int height = (plotRect.bottom - plotRect.top - 40);

            // trace altitude (courbe bleue)
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
            HPEN hOld = SelectObject(hdc, hPen);

            for (int i = 0; i < g_sampleCount; ++i) {
                float xNorm = (float)i / (float)(MAX_SAMPLES - 1);
                float yNorm = (float)(g_altHistory[i] - minAlt) /
                            (float)(maxAlt - minAlt);

                int x = plotRect.left + 40 + (int)(xNorm * width);
                int y = plotRect.bottom - 20 - (int)(yNorm * height);

                if (i == 0) {
                    MoveToEx(hdc, x, y, NULL);
                } else {
                    LineTo(hdc, x, y);
                }
            }

            SelectObject(hdc, hOld);
            DeleteObject(hPen);
        }

        EndPaint(hwnd, &ps);
        break;
    }

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    return 0;
}