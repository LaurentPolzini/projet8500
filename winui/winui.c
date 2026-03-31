// winui.c : interface Win32 pour le panneau avionique
// Corrections : double-buffering, graphe clippé, graduations Y alignées,
//               fond sombre style cockpit, pas de scintillement.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../pannel.h"
#include "../calculateur.h"

/* ------------------------------------------------------------------ */
/*  Constantes                                                          */
/* ------------------------------------------------------------------ */
#define MAX_SAMPLES         600
#define UPDATE_INTERVAL_MS   50

/* Couleurs du thème "cockpit sombre" */
#define CLR_BG          RGB( 18,  18,  35)   /* fond fenêtre          */
#define CLR_PANEL_BG    RGB( 28,  28,  50)   /* fond panneau gauche   */
#define CLR_PLOT_BG     RGB( 12,  12,  24)   /* fond graphe           */
#define CLR_GRID        RGB( 45,  45,  80)   /* grille                */
#define CLR_AXIS        RGB( 90,  90, 140)   /* axes                  */
#define CLR_CURVE       RGB( 30, 144, 255)   /* courbe altitude       */
#define CLR_TEXT_LABEL  RGB(160, 160, 200)   /* labels fixes          */
#define CLR_TEXT_VALUE  RGB(220, 240, 255)   /* valeurs dynamiques    */
#define CLR_ACCENT      RGB( 30, 144, 255)   /* accent bleu           */
#define CLR_BTN_BG      RGB( 40,  80, 160)
#define CLR_BTN_STOP    RGB(160,  40,  40)

/* IDs contrôles */
#define IDC_ALT_INPUT   101
#define IDC_TAUX_INPUT  102
#define IDC_ANGLE_INPUT 103
#define IDC_BTN_SEND    104
#define IDC_ALT_VALUE   201
#define IDC_TAUX_VALUE  202
#define IDC_PUISS_VALUE 203
#define IDC_MODE_VALUE  204

/* Dimensions fenêtre */
#define WIN_W  900
#define WIN_H  620

/* Zone panneau gauche (infos + inputs) */
#define PANEL_W  260

/* Marges internes du graphe */
#define PLOT_LEFT_MARGIN   70   /* espace pour labels Y              */
#define PLOT_RIGHT_MARGIN  20
#define PLOT_TOP_MARGIN    20
#define PLOT_BOT_MARGIN    30   /* espace pour labels X              */

/* ------------------------------------------------------------------ */
/*  État global                                                         */
/* ------------------------------------------------------------------ */
static int    g_altHistory[MAX_SAMPLES];
static int    g_sampleCount     = 0;
static int    g_simulationRunning = 0;

static Panel  g_panel      = NULL;
static HWND   g_hAltValue  = NULL;
static HWND   g_hTauxValue = NULL;
static HWND   g_hPuissValue= NULL;
static HWND   g_hModeValue = NULL;
static HWND   g_hAltInput  = NULL;
static HWND   g_hTauxInput = NULL;
static HWND   g_hAngleInput= NULL;
static HWND   g_hSendButton= NULL;

static HFONT  g_hFontLabel = NULL;
static HFONT  g_hFontValue = NULL;
static HFONT  g_hFontBig   = NULL;

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */
static void add_sample(int altitude)
{
    if (g_sampleCount < MAX_SAMPLES) {
        g_altHistory[g_sampleCount++] = altitude;
    } else {
        memmove(g_altHistory, g_altHistory + 1,
                (MAX_SAMPLES - 1) * sizeof(int));
        g_altHistory[MAX_SAMPLES - 1] = altitude;
    }
}

/* Choisit un palier de graduation "joli" selon l'amplitude */
static int nice_step(int range)
{
    if (range <= 500)    return 100;
    if (range <= 2000)   return 500;
    if (range <= 5000)   return 1000;
    if (range <= 10000)  return 2000;
    if (range <= 20000)  return 5000;
    return 10000;
}

/* Arrondit vers le bas au multiple de step */
static int floor_to(int v, int step) { return (v / step) * step; }

/* ------------------------------------------------------------------ */
/*  Mise à jour des STATIC labels (seulement les valeurs → pas de     */
/*  scintillement sur la zone contrôles)                               */
/* ------------------------------------------------------------------ */
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
        case MODE_AU_SOL:         modeStr = "AU SOL";        break;
        case MODE_CHANGEMENT_ALT: modeStr = "MONTEE/DESCENTE"; break;
        case MODE_VOL_CROISIERE:  modeStr = "VOL CROISIERE"; break;
    }
    SetWindowTextA(g_hModeValue, modeStr);
}

/* ------------------------------------------------------------------ */
/*  Dessin du graphe avec double-buffering                             */
/* ------------------------------------------------------------------ */
static void draw_graph(HDC hdc, RECT *plotRect)
{
    int pw = plotRect->right  - plotRect->left;
    int ph = plotRect->bottom - plotRect->top;

    /* --- Créer un bitmap mémoire de la taille exacte du plotRect --- */
    HDC     memDC  = CreateCompatibleDC(hdc);
    HBITMAP memBMP = CreateCompatibleBitmap(hdc, pw, ph);
    HBITMAP oldBMP = SelectObject(memDC, memBMP);

    /* Fond graphe */
    HBRUSH bgBrush = CreateSolidBrush(CLR_PLOT_BG);
    RECT localRect = { 0, 0, pw, ph };
    FillRect(memDC, &localRect, bgBrush);
    DeleteObject(bgBrush);

    /* Bordure fine */
    HPEN borderPen = CreatePen(PS_SOLID, 1, CLR_AXIS);
    HPEN oldPen    = (HPEN)SelectObject(memDC, borderPen);
    Rectangle(memDC, 0, 0, pw - 1, ph - 1);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    /* Zone de tracé (en coordonnées locales au bitmap) */
    int gx0 = PLOT_LEFT_MARGIN;
    int gx1 = pw - PLOT_RIGHT_MARGIN;
    int gy0 = PLOT_TOP_MARGIN;
    int gy1 = ph - PLOT_BOT_MARGIN;
    int gw  = gx1 - gx0;   /* largeur utile */
    int gh  = gy1 - gy0;   /* hauteur utile */

    if (g_sampleCount < 2) {
        /* Pas encore de données : message centré */
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, CLR_AXIS);
        SetTextAlign(memDC, TA_CENTER | TA_TOP);
        TextOutA(memDC, pw / 2, ph / 2 - 8, "En attente de donnees...", 24);
        goto blit;
    }

    {
        /* --- Calcul min/max altitude --- */
        int minAlt = g_altHistory[0], maxAlt = g_altHistory[0];
        for (int i = 1; i < g_sampleCount; ++i) {
            if (g_altHistory[i] < minAlt) minAlt = g_altHistory[i];
            if (g_altHistory[i] > maxAlt) maxAlt = g_altHistory[i];
        }

        /* Ajouter un peu de marge visuelle */
        int step = nice_step(maxAlt - minAlt + 1);
        minAlt = floor_to(minAlt, step);
        maxAlt = floor_to(maxAlt, step) + step;
        if (maxAlt <= minAlt) maxAlt = minAlt + step;

        int range = maxAlt - minAlt;

        /* --- Grille horizontale et labels Y --- */
        HPEN gridPen = CreatePen(PS_DOT, 1, CLR_GRID);
        HPEN axisPen = CreatePen(PS_SOLID, 1, CLR_AXIS);

        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, CLR_TEXT_LABEL);
        SetTextAlign(memDC, TA_RIGHT | TA_TOP);
        if (g_hFontLabel) SelectObject(memDC, g_hFontLabel);

        for (int alt = minAlt; alt <= maxAlt; alt += step) {
            /* y en coordonnées locales bitmap */
            int y = gy1 - (int)((float)(alt - minAlt) / range * gh);
            if (y < gy0 || y > gy1) continue;

            /* Ligne de grille */
            oldPen = (HPEN)SelectObject(memDC, gridPen);
            MoveToEx(memDC, gx0, y, NULL);
            LineTo  (memDC, gx1, y);
            SelectObject(memDC, oldPen);

            /* Label Y (aligné à droite sur gx0 - 4) */
            char label[16];
            snprintf(label, sizeof(label), "%d", alt);
            TextOutA(memDC, gx0 - 4, y - 7, label, (int)strlen(label));
        }
        DeleteObject(gridPen);

        /* --- Axes X et Y --- */
        oldPen = (HPEN)SelectObject(memDC, axisPen);
        MoveToEx(memDC, gx0, gy0, NULL);
        LineTo  (memDC, gx0, gy1);
        LineTo  (memDC, gx1, gy1);
        SelectObject(memDC, oldPen);
        DeleteObject(axisPen);

        /* Label axe Y */
        SetTextAlign(memDC, TA_CENTER | TA_TOP);
        TextOutA(memDC, gx0 / 2, gy0, "Alt (ft)", 8);

        /* Label axe X */
        SetTextAlign(memDC, TA_CENTER | TA_TOP);
        TextOutA(memDC, gx0 + gw / 2, gy1 + 4, "Temps (steps)", 13);

        /* --- Courbe altitude --- */
        HPEN curvePen = CreatePen(PS_SOLID, 2, CLR_CURVE);
        oldPen = (HPEN)SelectObject(memDC, curvePen);

        for (int i = 0; i < g_sampleCount; ++i) {
            /* x : on répartit les g_sampleCount points sur [gx0, gx1] */
            int x = gx0 + (int)((float)i / (float)(MAX_SAMPLES - 1) * gw);
            /* y : depuis le bas */
            int y = gy1 - (int)((float)(g_altHistory[i] - minAlt) / range * gh);

            /* Clamp interne par sécurité */
            if (x < gx0) x = gx0;
            if (x > gx1) x = gx1;
            if (y < gy0) y = gy0;
            if (y > gy1) y = gy1;

            if (i == 0) MoveToEx(memDC, x, y, NULL);
            else        LineTo  (memDC, x, y);
        }
        SelectObject(memDC, oldPen);
        DeleteObject(curvePen);
    }

blit:
    /* --- Recopier le bitmap mémoire vers le vrai HDC --- */
    BitBlt(hdc,
           plotRect->left, plotRect->top,
           pw, ph,
           memDC, 0, 0,
           SRCCOPY);

    SelectObject(memDC, oldBMP);
    DeleteObject(memBMP);
    DeleteDC(memDC);
}

/* ------------------------------------------------------------------ */
/*  handle_send                                                        */
/* ------------------------------------------------------------------ */
static void handle_send(HWND hwnd)
{
    if (g_simulationRunning) {
        KillTimer(hwnd, 1);
        g_simulationRunning = 0;
        SetWindowTextA(g_hSendButton, "Demarrer");
        return;
    }

    char buf[64];
    GetWindowTextA(g_hAltInput, buf, sizeof(buf));
    int alt_target = (int)strtol(buf, NULL, 10);

    if (!panel_set_altitude_desiree(g_panel, alt_target)) {
        MessageBoxA(hwnd, panel_get_last_error(g_panel), "Saisie invalide", MB_ICONWARNING);
        return;
    }

    /* Relance sans effacer l'historique (fenetre glissante MAX_SAMPLES) */
    g_simulationRunning = 1;
    SetWindowTextA(g_hSendButton, "Stop");
    SetTimer(hwnd, 1, UPDATE_INTERVAL_MS, NULL);
}

/* Callback pour EnumChildWindows : applique une HFONT à chaque enfant */
static BOOL CALLBACK set_font_proc(HWND h, LPARAM l)
{
    SendMessage(h, WM_SETFONT, (WPARAM)(HFONT)l, TRUE);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  WndProc                                                            */
/* ------------------------------------------------------------------ */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    /* -------- Création -------------------------------------------- */
    case WM_CREATE: {
        /* Polices */
        g_hFontLabel = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        g_hFontValue = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        g_hFontBig   = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");

        /* ---- Panneau gauche : affichages ---- */
        int lx = 10, col2 = 160, row = 20, dy = 32;

#define MAKE_LABEL(txt, y) \
    CreateWindowA("STATIC", txt, WS_VISIBLE|WS_CHILD|SS_LEFT, \
                  lx, y, 145, 20, hwnd, NULL, NULL, NULL)
#define MAKE_VALUE(id, txt, y) \
    CreateWindowA("STATIC", txt, WS_VISIBLE|WS_CHILD|SS_LEFT, \
                  col2, y, 95, 20, hwnd, (HMENU)(id), NULL, NULL)

        MAKE_LABEL("Altitude actuelle :",   row);
        g_hAltValue = MAKE_VALUE(IDC_ALT_VALUE,  "0 ft",       row); row += dy;

        MAKE_LABEL("Taux de montee :",      row);
        g_hTauxValue = MAKE_VALUE(IDC_TAUX_VALUE, "0.0 m/min", row); row += dy;

        MAKE_LABEL("Puissance moteur :",    row);
        g_hPuissValue = MAKE_VALUE(IDC_PUISS_VALUE, "0 %",     row); row += dy;

        MAKE_LABEL("Mode :",                row);
        g_hModeValue = MAKE_VALUE(IDC_MODE_VALUE, "AU SOL",    row); row += dy + 10;

        /* ---- Séparateur ---- */
        CreateWindowA("STATIC", "",
                      WS_VISIBLE|WS_CHILD|SS_ETCHEDHORZ,
                      lx, row, PANEL_W - 20, 2,
                      hwnd, NULL, NULL, NULL);
        row += 12;

        /* ---- Panneau gauche : entrées ---- */
#define MAKE_EDIT(id, def, y) \
    CreateWindowA("EDIT", def, \
                  WS_VISIBLE|WS_CHILD|WS_BORDER|ES_LEFT, \
                  col2, y, 80, 22, hwnd, (HMENU)(id), NULL, NULL)

        MAKE_LABEL("Altitude desiree (ft) :", row);
        g_hAltInput  = MAKE_EDIT(IDC_ALT_INPUT,  "",  row); row += dy;

        MAKE_LABEL("Taux montee (m/min) :",   row);
        g_hTauxInput  = MAKE_EDIT(IDC_TAUX_INPUT, "0", row); row += dy;

        MAKE_LABEL("Angle attaque (deg) :",   row);
        g_hAngleInput = MAKE_EDIT(IDC_ANGLE_INPUT, "0", row); row += dy + 10;

        /* ---- Bouton ---- */
        g_hSendButton = CreateWindowA("BUTTON", "Demarrer",
            WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
            lx, row, 120, 32,
            hwnd, (HMENU)IDC_BTN_SEND, NULL, NULL);

        /* Appliquer la police à tous les contrôles enfants */
        EnumChildWindows(hwnd, set_font_proc, (LPARAM)g_hFontLabel);

        update_display_from_panel();
        break;
    }

    /* -------- Timer : mise à jour simulation ----------------------- */
    case WM_TIMER:
        if (wParam == 1 && g_simulationRunning) {
            PanelInputs  in_v  = panel_get_inputs(g_panel);
            PanelDisplay st_v  = panel_get_display(g_panel);
            CalcInput    cin   = { in_v, st_v };
            CalcOutput   cout;
            calculateur_run(&cin, &cout);
            panel_set_display(g_panel, &cout.state_out);

            PanelDisplay outd = panel_get_display(g_panel);
            add_sample(outd.altitude_ft);
            update_display_from_panel();  /* met à jour seulement les STATIC */

            /* Fin automatique */
            if (outd.mode == MODE_VOL_CROISIERE) {
                KillTimer(hwnd, 1);
                g_simulationRunning = 0;
                SetWindowTextA(g_hSendButton, "Demarrer");
            }

            /* Invalider SEULEMENT la zone graphe, pas les contrôles */
            RECT graphOnly;
            GetClientRect(hwnd, &graphOnly);
            graphOnly.left = PANEL_W + 10;   /* bande gauche exclue */
            InvalidateRect(hwnd, &graphOnly, FALSE); /* FALSE = pas d'effacement */
        }
        break;

    /* -------- Commandes bouton ------------------------------------- */
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_SEND &&
            HIWORD(wParam) == BN_CLICKED) {
            handle_send(hwnd);
        }
        break;

    /* -------- Dessin ----------------------------------------------- */
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        /* -- Fond global (bande droite uniquement pour la perf) -- */
        HBRUSH bgBr = CreateSolidBrush(CLR_BG);
        RECT rightArea = { PANEL_W, 0, rc.right, rc.bottom };
        FillRect(hdc, &rightArea, bgBr);
        DeleteObject(bgBr);

        /* -- Fond panneau gauche -- */
        HBRUSH panelBr = CreateSolidBrush(CLR_PANEL_BG);
        RECT leftArea = { 0, 0, PANEL_W, rc.bottom };
        FillRect(hdc, &leftArea, panelBr);
        DeleteObject(panelBr);

        /* -- Ligne de séparation verticale -- */
        HPEN sepPen = CreatePen(PS_SOLID, 2, CLR_ACCENT);
        HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
        MoveToEx(hdc, PANEL_W, 0,        NULL);
        LineTo  (hdc, PANEL_W, rc.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(sepPen);

        /* -- Titre graphe -- */
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_TEXT_LABEL);
        SetTextAlign(hdc, TA_LEFT | TA_TOP);
        if (g_hFontBig) SelectObject(hdc, g_hFontBig);
        TextOutA(hdc, PANEL_W + 14, 12, "Profil d'altitude", 17);

        /* -- Zone graphe : remplit le reste avec marges -- */
        RECT plotRect = {
            PANEL_W + 10,
            40,
            rc.right  - 10,
            rc.bottom - 10
        };

        /* S'assurer que la zone est assez grande */
        if (plotRect.right  > plotRect.left + 50 &&
            plotRect.bottom > plotRect.top  + 50) {
            draw_graph(hdc, &plotRect);
        }

        EndPaint(hwnd, &ps);
        break;
    }

    /* -------- Redimensionnement : forcer un repaint complet -------- */
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    /* -------- Couleurs des contrôles texte (fond sombre) ----------- */
    case WM_CTLCOLORSTATIC: {
        HDC hdcCtrl = (HDC)wParam;
        SetTextColor(hdcCtrl, CLR_TEXT_VALUE);
        SetBkColor  (hdcCtrl, CLR_PANEL_BG);
        static HBRUSH s_panelBrush = NULL;
        if (!s_panelBrush) s_panelBrush = CreateSolidBrush(CLR_PANEL_BG);
        return (LRESULT)s_panelBrush;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdcCtrl = (HDC)wParam;
        SetTextColor(hdcCtrl, CLR_TEXT_VALUE);
        SetBkColor  (hdcCtrl, RGB(30, 30, 55));
        static HBRUSH s_editBrush = NULL;
        if (!s_editBrush) s_editBrush = CreateSolidBrush(RGB(30, 30, 55));
        return (LRESULT)s_editBrush;
    }

    /* -------- Destruction ------------------------------------------ */
    case WM_DESTROY:
        if (g_hFontLabel) { DeleteObject(g_hFontLabel); g_hFontLabel = NULL; }
        if (g_hFontValue) { DeleteObject(g_hFontValue); g_hFontValue = NULL; }
        if (g_hFontBig)   { DeleteObject(g_hFontBig);   g_hFontBig   = NULL; }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  WinMain                                                            */
/* ------------------------------------------------------------------ */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    g_panel = panel_init();
    if (!g_panel) return 1;

    WNDCLASSA wc     = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "AvioniquePanelClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); /* évite flash blanc */
    wc.style         = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Echec RegisterClass", "Erreur", MB_ICONERROR);
        return 1;
    }

    /* Calculer la taille de fenêtre cliente exacte */
    RECT wr = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowA(
        "AvioniquePanelClass",
        "Panneau avionique",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
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