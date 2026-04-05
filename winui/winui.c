// winui.c - Panneau avionique : interface style cockpit
// 6 zones : saisie | envoi | instruments | AOA | etat avionique | graphe

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../pannel.h"
#include "../calculateur.h"

/* ================================================================== */
/*  CONSTANTES                                                          */
/* ================================================================== */
#define MAX_SAMPLES        600
#define UPDATE_INTERVAL_MS  50

/* Palette */
#define CLR_BG          RGB( 22,  22,  30)
#define CLR_CELL_BG     RGB( 30,  32,  42)
#define CLR_CELL_BORDER RGB( 55,  58,  75)
#define CLR_GREEN       RGB( 39, 201, 139)
#define CLR_GREEN_DIM   RGB( 25, 120,  80)
#define CLR_PLOT_BG     RGB( 15,  16,  22)
#define CLR_GRID        RGB( 40,  42,  58)
#define CLR_AXIS        RGB( 80,  85, 110)
#define CLR_CURVE       RGB( 39, 201, 139)
#define CLR_TEXT        RGB(220, 225, 240)
#define CLR_TEXT_DIM    RGB(120, 125, 150)
#define CLR_TEXT_GREEN  RGB( 39, 201, 139)
#define CLR_SKY         RGB( 70, 160, 220)
#define CLR_EARTH       RGB(140,  90,  50)
#define CLR_WHITE       RGB(255, 255, 255)

/* IDs controles natifs */
#define IDC_ALT_INPUT   101
#define IDC_TAUX_INPUT  102
#define IDC_ANGLE_INPUT 103
#define IDC_BTN_SEND    104

/* Layout fenetre : 1280 x 720 */
#define WIN_W  1280
#define WIN_H   720

/* Colonne gauche (saisie + bouton) */
#define COL_LEFT_W   310

/* Grille droite : 2 lignes x 3 colonnes */
#define GRID_X      (COL_LEFT_W + 1)
#define GRID_W      (WIN_W - GRID_X)
#define CELL_W      (GRID_W / 3)
#define ROW_TOP_H   360
#define ROW_BOT_Y   (ROW_TOP_H + 1)
#define ROW_BOT_H   (WIN_H - ROW_BOT_Y)

/* Marges */
#define PAD  10

/* Graphe : marges internes */
#define GP_L  65
#define GP_R  15
#define GP_T  30
#define GP_B  35

/* ================================================================== */
/*  ETAT GLOBAL                                                         */
/* ================================================================== */
static int   g_altHistory[MAX_SAMPLES];
static int   g_sampleCount       = 0;
static int   g_simulationRunning = 0;

static Panel g_panel      = NULL;
static HWND  g_hAltInput  = NULL;
static HWND  g_hTauxInput = NULL;
static HWND  g_hAngleInput= NULL;
static HWND  g_hSendBtn   = NULL;

static HFONT g_fntSmall   = NULL;
static HFONT g_fntNormal  = NULL;
static HFONT g_fntBold    = NULL;
static HFONT g_fntBig     = NULL;
static HFONT g_fntHuge    = NULL;

/* ================================================================== */
/*  UTILITAIRES                                                         */
/* ================================================================== */
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

static int nice_step(int range)
{
    if (range <=   500) return  100;
    if (range <=  2000) return  500;
    if (range <=  5000) return 1000;
    if (range <= 10000) return 2000;
    if (range <= 20000) return 5000;
    return 10000;
}

static int floor_to(int v, int step)
{
    if (v < 0) return ((v - step + 1) / step) * step;
    return (v / step) * step;
}

static void fill_rounded_rect(HDC hdc, RECT *r, int rx, COLORREF col)
{
    HBRUSH br = CreateSolidBrush(col);
    HPEN   pn = CreatePen(PS_SOLID, 1, col);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    HPEN   op = (HPEN)  SelectObject(hdc, pn);
    RoundRect(hdc, r->left, r->top, r->right, r->bottom, rx, rx);
    SelectObject(hdc, ob); DeleteObject(br);
    SelectObject(hdc, op); DeleteObject(pn);
}

static void draw_text_center(HDC hdc, const char *s, RECT *r,
                              HFONT f, COLORREF c)
{
    if (f) SelectObject(hdc, f);
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextA(hdc, s, -1, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_text_left(HDC hdc, const char *s, int x, int y,
                            HFONT f, COLORREF c)
{
    if (f) SelectObject(hdc, f);
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    SetTextAlign(hdc, TA_TOP | TA_LEFT);
    TextOutA(hdc, x, y, s, (int)strlen(s));
}

static void draw_text_right(HDC hdc, const char *s, int x, int y,
                             HFONT f, COLORREF c)
{
    if (f) SelectObject(hdc, f);
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    SetTextAlign(hdc, TA_TOP | TA_RIGHT);
    TextOutA(hdc, x, y, s, (int)strlen(s));
}

static void draw_cell_bg(HDC hdc, RECT *r)
{
    HBRUSH br = CreateSolidBrush(CLR_CELL_BG);
    FillRect(hdc, r, br);
    DeleteObject(br);
    HPEN pn = CreatePen(PS_SOLID, 1, CLR_CELL_BORDER);
    HPEN op = (HPEN)SelectObject(hdc, pn);
    HBRUSH nb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r->left, r->top, r->right, r->bottom);
    SelectObject(hdc, op); DeleteObject(pn);
    SelectObject(hdc, nb);
}

/* ================================================================== */
/*  ZONE INSTRUMENTS (ALT + puissance + VVI)                           */
/* ================================================================== */
static void draw_instruments(HDC hdc, RECT *cell, PanelDisplay *d)
{
    draw_cell_bg(hdc, cell);

    int cw = cell->right - cell->left;
    int ch = cell->bottom - cell->top;
    int cx = cell->left + cw / 2;
    int cy = cell->top  + ch / 2 - 10;

    /* --- Cercle altimetre --- */
    int r = 95;
    {
        HBRUSH brDark = CreateSolidBrush(RGB(14, 16, 24));
        HPEN   pnBord = CreatePen(PS_SOLID, 2, CLR_CELL_BORDER);
        HPEN   op     = (HPEN)  SelectObject(hdc, pnBord);
        HBRUSH ob     = (HBRUSH)SelectObject(hdc, brDark);
        Ellipse(hdc, cx-r, cy-r, cx+r, cy+r);
        SelectObject(hdc, op); DeleteObject(pnBord);
        SelectObject(hdc, ob); DeleteObject(brDark);
    }

    /* Arc de progression */
    {
        PanelInputs ins = panel_get_inputs(g_panel);
        int target = ins.altitude_desiree_ft;
        float pct = (target > 0) ? ((float)d->altitude_ft / target) : 1.0f;
        if (pct > 1.0f) pct = 1.0f;
        if (pct < 0.0f) pct = 0.0f;

        int arcR = r + 10;
        float startA = -1.5707963f; /* -pi/2 = 12h */
        float sweep  = pct * 6.2831853f;

        #define ARC_N 72
        POINT pts[ARC_N + 1];
        for (int i = 0; i <= ARC_N; ++i) {
            float a = startA + sweep * i / ARC_N;
            pts[i].x = cx + (int)(cosf(a) * arcR);
            pts[i].y = cy + (int)(sinf(a) * arcR);
        }
        HPEN pnArc = CreatePen(PS_SOLID, 4, CLR_GREEN);
        HPEN op    = (HPEN)SelectObject(hdc, pnArc);
        Polyline(hdc, pts, ARC_N + 1);
        SelectObject(hdc, op); DeleteObject(pnArc);
    }

    /* Badge "ALT" */
    {
        RECT badge = { cx - 40, cy - 28, cx + 40, cy - 8 };
        fill_rounded_rect(hdc, &badge, 6, CLR_GREEN);
        draw_text_center(hdc, "ALT", &badge, g_fntBold, CLR_BG);
    }

    /* Valeur altitude */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%dpi", d->altitude_ft);
        RECT rv = { cx - 80, cy - 4, cx + 80, cy + 44 };
        draw_text_center(hdc, buf, &rv, g_fntHuge, CLR_TEXT);
    }

    /* --- Puissance (bas gauche) --- */
    {
        int bx = cell->left + PAD;
        int by = cell->bottom - 65;
        int bw = 14, bh = 50, nBars = 8;
        int filled = (int)(d->puissance_pct / 100.0f * nBars + 0.5f);

        for (int i = 0; i < nBars; ++i) {
            COLORREF c = (i < filled) ? CLR_GREEN : CLR_GRID;
            HBRUSH b = CreateSolidBrush(c);
            int yb = by + bh - (i + 1) * (bh / nBars);
            RECT rb = { bx, yb, bx + bw, yb + bh / nBars - 2 };
            FillRect(hdc, &rb, b);
            DeleteObject(b);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f%%", d->puissance_pct);
        draw_text_left(hdc, "POW", bx, by - 30, g_fntNormal, CLR_TEXT_DIM);
        draw_text_left(hdc, buf,   bx, by - 13, g_fntBold,   CLR_TEXT_GREEN);
    }

    /* --- VVI (bas droite) --- */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fm/min", d->vitesse_mpm);
        int rx2 = cell->right - PAD;
        int by  = cell->bottom - 65;
        draw_text_right(hdc, "VVI", rx2, by - 30, g_fntNormal, CLR_TEXT_DIM);
        draw_text_right(hdc, buf,   rx2, by - 13, g_fntBold,   CLR_TEXT_GREEN);
    }
}

/* ================================================================== */
/*  ZONE AOA (Attitude Indicator)                                       */
/* ================================================================== */
static void draw_aoa(HDC hdc, RECT *cell, PanelDisplay *d)
{
    draw_cell_bg(hdc, cell);

    int cw = cell->right - cell->left;
    int ch = cell->bottom - cell->top;
    int cx = cell->left + cw / 2;
    int cy = cell->top  + ch / 2 + 15;
    int r  = (cw < ch ? cw : ch) / 2 - 30;

    draw_text_left(hdc, "AOA", cell->left + PAD, cell->top + PAD,
                   g_fntBold, CLR_TEXT);

    /* Angle */
    PanelInputs ins = panel_get_inputs(g_panel);
    float angle_deg = ins.angle_deg;
    if (angle_deg == 0.0f) {
        if (d->vitesse_mpm > 0)      angle_deg =  2.3f;
        else if (d->vitesse_mpm < 0) angle_deg = -2.3f;
    }

    char angbuf[16];
    snprintf(angbuf, sizeof(angbuf), "%.1f", angle_deg);
    draw_text_right(hdc, angbuf,   cell->right - PAD, cell->top + PAD,
                    g_fntBold, CLR_TEXT);
    draw_text_right(hdc, "deg",    cell->right - PAD, cell->top + PAD + 20,
                    g_fntSmall, CLR_TEXT_DIM);

    /* Clip circulaire */
    HRGN clip = CreateEllipticRgn(cx-r, cy-r, cx+r, cy+r);
    SelectClipRgn(hdc, clip);

    float angle_rad    = angle_deg * 3.14159f / 180.0f;
    int   hoff         = (int)(sinf(angle_rad) * r * 1.5f);

    /* Ciel */
    HBRUSH brSky = CreateSolidBrush(CLR_SKY);
    RECT rSky = { cx-r, cy-r, cx+r, cy + hoff };
    FillRect(hdc, &rSky, brSky);
    DeleteObject(brSky);

    /* Terre */
    HBRUSH brEarth = CreateSolidBrush(CLR_EARTH);
    RECT rEarth = { cx-r, cy + hoff, cx+r, cy+r };
    FillRect(hdc, &rEarth, brEarth);
    DeleteObject(brEarth);

    /* Horizon */
    HPEN pnH = CreatePen(PS_SOLID, 3, CLR_GREEN);
    HPEN opn = (HPEN)SelectObject(hdc, pnH);
    MoveToEx(hdc, cx-r, cy + hoff, NULL);
    LineTo  (hdc, cx+r, cy + hoff);
    SelectObject(hdc, opn); DeleteObject(pnH);

    /* Graduations */
    HPEN pnW = CreatePen(PS_SOLID, 1, CLR_WHITE);
    opn = (HPEN)SelectObject(hdc, pnW);
    int grads[] = {-20, -10, 10, 20};
    for (int i = 0; i < 4; ++i) {
        int gy2 = cy + hoff - (int)(grads[i] * r / 20.0f);
        int hw  = (abs(grads[i]) == 20) ? r/2 : r/3;
        MoveToEx(hdc, cx - hw, gy2, NULL);
        LineTo  (hdc, cx + hw, gy2);
        char gl[8];
        snprintf(gl, sizeof(gl), "%d", abs(grads[i]));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_WHITE);
        if (g_fntSmall) SelectObject(hdc, g_fntSmall);
        SetTextAlign(hdc, TA_TOP | TA_RIGHT);
        TextOutA(hdc, cx - hw - 3, gy2 - 7, gl, (int)strlen(gl));
        SetTextAlign(hdc, TA_TOP | TA_LEFT);
        TextOutA(hdc, cx + hw + 3, gy2 - 7, gl, (int)strlen(gl));
    }
    SelectObject(hdc, opn); DeleteObject(pnW);

    /* Repere avion */
    HPEN pnCross = CreatePen(PS_SOLID, 2, CLR_GREEN);
    opn = (HPEN)SelectObject(hdc, pnCross);
    int hy = cy + hoff;
    MoveToEx(hdc, cx - 30, hy, NULL); LineTo(hdc, cx - 10, hy);
    LineTo  (hdc, cx - 10, hy - 8);
    MoveToEx(hdc, cx + 30, hy, NULL); LineTo(hdc, cx + 10, hy);
    LineTo  (hdc, cx + 10, hy - 8);
    MoveToEx(hdc, cx,      hy, NULL); LineTo(hdc, cx,      hy - 6);
    SelectObject(hdc, opn); DeleteObject(pnCross);

    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);

    /* Bordure cercle */
    HPEN pnBrd = CreatePen(PS_SOLID, 2, CLR_CELL_BORDER);
    opn = (HPEN)SelectObject(hdc, pnBrd);
    HBRUSH nb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx-r, cy-r, cx+r, cy+r);
    SelectObject(hdc, opn); DeleteObject(pnBrd);
    SelectObject(hdc, nb);
}

static void draw_avionics_state(HDC hdc, RECT *cell, PanelDisplay *d)
{
    draw_cell_bg(hdc, cell);

    int cw    = cell->right - cell->left;
    int ch    = cell->bottom - cell->top;
    int col_w = cw / 3;

    const char *labels[] = { "AU SOL", "CHANGEMENT ALT", "VOL CROISIERE" };
    PanelMode   modes [] = { MODE_AU_SOL, MODE_CHANGEMENT_ALT, MODE_VOL_CROISIERE };

    for (int i = 0; i < 3; ++i) {
        int      active = (d->mode == modes[i]);
        COLORREF col    = active ? CLR_GREEN : CLR_TEXT_DIM;

        int icy = cell->top  + ch / 2 - 25;

        RECT lr = { cell->left + col_w * i + 4,
                    icy + 50,
                    cell->left + col_w * (i + 1) - 4,
                    icy + 72 };
        draw_text_center(hdc, labels[i], &lr,
                         active ? g_fntBold : g_fntSmall, col);
    }
}

/* ================================================================== */
/*  ZONE GRAPHE                                                         */
/* ================================================================== */
static void draw_graph(HDC hdc, RECT *cell)
{
    int pw = cell->right - cell->left;
    int ph = cell->bottom - cell->top;

    HDC     mDC  = CreateCompatibleDC(hdc);
    HBITMAP mBMP = CreateCompatibleBitmap(hdc, pw, ph);
    HBITMAP oBMP = (HBITMAP)SelectObject(mDC, mBMP);

    HBRUSH bgBr = CreateSolidBrush(CLR_PLOT_BG);
    RECT lr = { 0, 0, pw, ph };
    FillRect(mDC, &lr, bgBr);
    DeleteObject(bgBr);

    HPEN brdPen = CreatePen(PS_SOLID, 1, CLR_CELL_BORDER);
    HPEN opn    = (HPEN)  SelectObject(mDC, brdPen);
    HBRUSH onbr = (HBRUSH)SelectObject(mDC, GetStockObject(NULL_BRUSH));
    Rectangle(mDC, 0, 0, pw-1, ph-1);
    SelectObject(mDC, opn); DeleteObject(brdPen);
    SelectObject(mDC, onbr);

    int gx0 = GP_L, gx1 = pw - GP_R;
    int gy0 = GP_T, gy1 = ph - GP_B;
    int gw  = gx1 - gx0, gh = gy1 - gy0;

    /* Titres axes */
    SetBkMode(mDC, TRANSPARENT);
    SetTextColor(mDC, CLR_TEXT_DIM);
    SetTextAlign(mDC, TA_LEFT | TA_TOP);
    if (g_fntSmall) SelectObject(mDC, g_fntSmall);
    TextOutA(mDC, gx0, 8, "ALTITUDE (PIEDS)", 16);
    SetTextAlign(mDC, TA_CENTER | TA_TOP);
    TextOutA(mDC, gx0 + gw/2, ph - GP_B + 10, "TEMPS (MIN)", 11);

    if (g_sampleCount < 2) {
        SetTextColor(mDC, CLR_AXIS);
        SetTextAlign(mDC, TA_CENTER | TA_TOP);
        TextOutA(mDC, pw/2, ph/2 - 8, "En attente de donnees...", 24);
        goto blit;
    }

    {
        int minAlt = g_altHistory[0], maxAlt = g_altHistory[0];
        for (int i = 1; i < g_sampleCount; ++i) {
            if (g_altHistory[i] < minAlt) minAlt = g_altHistory[i];
            if (g_altHistory[i] > maxAlt) maxAlt = g_altHistory[i];
        }
        int step = nice_step(maxAlt - minAlt + 1);
        minAlt = floor_to(minAlt, step);
        maxAlt = floor_to(maxAlt, step) + step;
        if (maxAlt <= minAlt) maxAlt = minAlt + step;
        int range = maxAlt - minAlt;

        /* Grille + labels Y */
        HPEN gridPen = CreatePen(PS_DOT, 1, CLR_GRID);
        HPEN axisPen = CreatePen(PS_SOLID, 1, CLR_AXIS);

        SetBkMode(mDC, TRANSPARENT);
        SetTextColor(mDC, CLR_TEXT_DIM);
        if (g_fntSmall) SelectObject(mDC, g_fntSmall);

        for (int alt = minAlt; alt <= maxAlt; alt += step) {
            int y = gy1 - (int)((float)(alt - minAlt) / range * gh);
            if (y < gy0 || y > gy1) continue;

            opn = (HPEN)SelectObject(mDC, gridPen);
            MoveToEx(mDC, gx0, y, NULL); LineTo(mDC, gx1, y);
            SelectObject(mDC, opn);

            char lbl[16];
            snprintf(lbl, sizeof(lbl), "%d", alt);
            SetTextAlign(mDC, TA_RIGHT | TA_TOP);
            TextOutA(mDC, gx0 - 4, y - 7, lbl, (int)strlen(lbl));
        }
        DeleteObject(gridPen);

        /* Axes */
        opn = (HPEN)SelectObject(mDC, axisPen);
        MoveToEx(mDC, gx0, gy0, NULL);
        LineTo  (mDC, gx0, gy1);
        LineTo  (mDC, gx1, gy1);
        SelectObject(mDC, opn); DeleteObject(axisPen);

        /* Labels X (temps en minutes) */
        {
            float dt_min = (float)UPDATE_INTERVAL_MS / 60000.0f;
            float total  = g_sampleCount * dt_min;
            if (total < 0.01f) total = 0.01f;
            int tmax = (int)total + 1;
            SetTextColor(mDC, CLR_TEXT_DIM);
            SetTextAlign(mDC, TA_CENTER | TA_TOP);
            if (g_fntSmall) SelectObject(mDC, g_fntSmall);
            int tick_step = (tmax > 10) ? 2 : 1;
            for (int t = 0; t <= tmax; t += tick_step) {
                int x = gx0 + (int)((float)t / total * gw);
                if (x > gx1) x = gx1;
                char lbl[8];
                snprintf(lbl, sizeof(lbl), "%d", t);
                TextOutA(mDC, x, gy1 + 4, lbl, (int)strlen(lbl));
            }
        }

        /* Courbe */
        HPEN curvePen = CreatePen(PS_SOLID, 1, CLR_CURVE);
        opn = (HPEN)SelectObject(mDC, curvePen);

        POINT prev = {-1, -1};
        for (int i = 0; i < g_sampleCount; ++i) {
            int x = gx0 + (int)((float)i / (MAX_SAMPLES - 1) * gw);
            int y = gy1 - (int)((float)(g_altHistory[i] - minAlt) / range * gh);
            if (x < gx0) x = gx0; 
            if (x > gx1) x = gx1;
            if (y < gy0) y = gy0; 
            if (y > gy1) y = gy1;

            if (prev.x >= 0) {
                MoveToEx(mDC, prev.x, prev.y, NULL);
                LineTo  (mDC, x, y);
            }
            /* Marqueur '+' */
            if (i % 10 == 0) {
                MoveToEx(mDC, x-4, y,   NULL); LineTo(mDC, x+4, y);
                MoveToEx(mDC, x,   y-4, NULL); LineTo(mDC, x,   y+4);
            }
            prev.x = x; prev.y = y;
        }
        SelectObject(mDC, opn); DeleteObject(curvePen);
    }

blit:
    BitBlt(hdc, cell->left, cell->top, pw, ph, mDC, 0, 0, SRCCOPY);
    SelectObject(mDC, oBMP);
    DeleteObject(mBMP);
    DeleteDC(mDC);
}

/* ================================================================== */
/*  HANDLE SEND                                                         */
/* ================================================================== */
static void handle_send(HWND hwnd)
{
    if (g_simulationRunning) {
        KillTimer(hwnd, 1);
        g_simulationRunning = 0;
        SetWindowTextA(g_hSendBtn, "Envoyer");
        return;
    }

    char buf[64];

    GetWindowTextA(g_hAltInput,   buf, sizeof(buf));
    int   alt   = (int)strtol(buf, NULL, 10);

    GetWindowTextA(g_hTauxInput,  buf, sizeof(buf));
    float taux  = (float)strtod(buf, NULL);

    GetWindowTextA(g_hAngleInput, buf, sizeof(buf));
    float angle = (float)strtod(buf, NULL);

    if (!panel_set_altitude_desiree(g_panel, alt)) {
        MessageBoxA(hwnd, panel_get_last_error(g_panel),
                    "Saisie invalide", MB_ICONWARNING);
        return;
    }
    panel_set_taux_montee(g_panel, taux);
    panel_set_angle(g_panel, angle);

    g_simulationRunning = 1;
    SetWindowTextA(g_hSendBtn, "Stop");
    SetTimer(hwnd, 1, UPDATE_INTERVAL_MS, NULL);
}

/* ================================================================== */
/*  DESSIN PANNEAU GAUCHE                                               */
/* ================================================================== */
static void paint_left(HDC hdc, HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);

    HBRUSH br = CreateSolidBrush(CLR_CELL_BG);
    RECT la = { 0, 0, COL_LEFT_W, rc.bottom };
    FillRect(hdc, &la, br);
    DeleteObject(br);

    HPEN pn = CreatePen(PS_SOLID, 1, CLR_CELL_BORDER);
    HPEN op = (HPEN)SelectObject(hdc, pn);
    MoveToEx(hdc, COL_LEFT_W, 0, NULL);
    LineTo  (hdc, COL_LEFT_W, rc.bottom);
    SelectObject(hdc, op); DeleteObject(pn);

    /* Recapitulatif en bas */
    PanelInputs ins = panel_get_inputs(g_panel);
    char buf[128];
    int ry = rc.bottom - 95;

    HPEN sep = CreatePen(PS_SOLID, 1, CLR_CELL_BORDER);
    op = (HPEN)SelectObject(hdc, sep);
    MoveToEx(hdc, PAD, ry - 8, NULL);
    LineTo  (hdc, COL_LEFT_W - PAD, ry - 8);
    SelectObject(hdc, op); DeleteObject(sep);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_TEXT_DIM);
    if (g_fntSmall) SelectObject(hdc, g_fntSmall);
    SetTextAlign(hdc, TA_LEFT | TA_TOP);

    snprintf(buf, sizeof(buf), "Altitude = %d", ins.altitude_desiree_ft);
    TextOutA(hdc, PAD, ry,      buf, (int)strlen(buf));
    snprintf(buf, sizeof(buf), "Taux de montee = %.0f", ins.taux_montee_mpm);
    TextOutA(hdc, PAD, ry + 18, buf, (int)strlen(buf));
    snprintf(buf, sizeof(buf), "Angle d'attaque = %.1f", ins.angle_deg);
    TextOutA(hdc, PAD, ry + 36, buf, (int)strlen(buf));
}

/* ================================================================== */
/*  DESSIN ZONE DROITE                                                  */
/* ================================================================== */
static void paint_right(HDC hdc, HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    PanelDisplay d = panel_get_display(g_panel);

    HBRUSH bgBr = CreateSolidBrush(CLR_BG);
    RECT ra = { GRID_X, 0, rc.right, rc.bottom };
    FillRect(hdc, &ra, bgBr);
    DeleteObject(bgBr);

    RECT cInstr = { GRID_X,              0, GRID_X + CELL_W,     ROW_TOP_H };
    RECT cAoa   = { GRID_X + CELL_W,     0, GRID_X + CELL_W * 2, ROW_TOP_H };
    RECT cMode  = { GRID_X + CELL_W * 2, 0, rc.right,             ROW_TOP_H };
    RECT cGraph = { GRID_X, ROW_BOT_Y, rc.right, rc.bottom };

    draw_instruments   (hdc, &cInstr, &d);
    draw_aoa           (hdc, &cAoa,   &d);
    draw_avionics_state(hdc, &cMode,  &d);
    draw_graph         (hdc, &cGraph);
}

/* ================================================================== */
/*  set_font_proc                                                       */
/* ================================================================== */
static BOOL CALLBACK set_font_proc(HWND h, LPARAM l)
{
    SendMessage(h, WM_SETFONT, (WPARAM)(HFONT)l, TRUE);
    return TRUE;
}

/* ================================================================== */
/*  WndProc                                                             */
/* ================================================================== */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE: {
        g_fntSmall  = CreateFontA(12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                        DEFAULT_PITCH|FF_SWISS,"Segoe UI");
        g_fntNormal = CreateFontA(14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                        DEFAULT_PITCH|FF_SWISS,"Segoe UI");
        g_fntBold   = CreateFontA(16,0,0,0,FW_BOLD,  0,0,0,DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                        DEFAULT_PITCH|FF_SWISS,"Segoe UI");
        g_fntBig    = CreateFontA(26,0,0,0,FW_BOLD,  0,0,0,DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                        DEFAULT_PITCH|FF_SWISS,"Segoe UI");
        g_fntHuge   = CreateFontA(36,0,0,0,FW_BOLD,  0,0,0,DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                        DEFAULT_PITCH|FF_SWISS,"Segoe UI");

        int lx = PAD, ew = COL_LEFT_W - lx * 2, row = 20, dy = 62;

#define MAKE_STATIC(txt, y) \
    CreateWindowA("STATIC", txt, WS_VISIBLE|WS_CHILD|SS_LEFT, \
                  lx, y, ew, 18, hwnd, NULL, NULL, NULL)
#define MAKE_EDIT2(id, txt, y) \
    CreateWindowA("EDIT", txt, \
        WS_VISIBLE|WS_CHILD|WS_BORDER|ES_LEFT|ES_AUTOHSCROLL, \
        lx, (y)+20, ew, 28, hwnd, (HMENU)(UINT_PTR)(id), NULL, NULL)

        MAKE_STATIC("Altitude desiree (en pieds)",        row);
        g_hAltInput   = MAKE_EDIT2(IDC_ALT_INPUT,   "",  row); row += dy;

        MAKE_STATIC("Taux de montee desire (en m/min)",   row);
        g_hTauxInput  = MAKE_EDIT2(IDC_TAUX_INPUT,  "0", row); row += dy;

        MAKE_STATIC("Angle d'attaque desire (en degre)",  row);
        g_hAngleInput = MAKE_EDIT2(IDC_ANGLE_INPUT, "0", row); row += dy + 10;

        g_hSendBtn = CreateWindowA("BUTTON", "Envoyer",
            WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
            lx, row, ew, 55,
            hwnd, (HMENU)(UINT_PTR)IDC_BTN_SEND, NULL, NULL);

        EnumChildWindows(hwnd, set_font_proc, (LPARAM)g_fntNormal);
        SendMessage(g_hSendBtn, WM_SETFONT, (WPARAM)g_fntBig, TRUE);
        break;
    }

    case WM_TIMER:
        if (wParam == 1 && g_simulationRunning) {
            PanelInputs  iv = panel_get_inputs(g_panel);
            PanelDisplay sv = panel_get_display(g_panel);
            CalcInput  cin  = { iv, sv };
            CalcOutput cout;
            calculateur_run(&cin, &cout);
            panel_set_display(g_panel, &cout.state_out);

            PanelDisplay od = panel_get_display(g_panel);
            add_sample(od.altitude_ft);

            if (od.mode == MODE_VOL_CROISIERE) {
                KillTimer(hwnd, 1);
                g_simulationRunning = 0;
                SetWindowTextA(g_hSendBtn, "Envoyer");
            }

            /* Invalider zone droite + bas panneau gauche */
            RECT rc; GetClientRect(hwnd, &rc);
            RECT right = { GRID_X, 0, rc.right, rc.bottom };
            InvalidateRect(hwnd, &right, FALSE);
            RECT bot = { 0, rc.bottom - 110, COL_LEFT_W, rc.bottom };
            InvalidateRect(hwnd, &bot, FALSE);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_SEND &&
            HIWORD(wParam) == BN_CLICKED) {
            handle_send(hwnd);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc; GetClientRect(hwnd, &rc);
        HDC     mDC  = CreateCompatibleDC(hdc);
        HBITMAP mBMP = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oBMP = (HBITMAP)SelectObject(mDC, mBMP);

        paint_left (mDC, hwnd);
        paint_right(mDC, hwnd);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mDC, 0, 0, SRCCOPY);
        SelectObject(mDC, oBMP);
        DeleteObject(mBMP);
        DeleteDC(mDC);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_CTLCOLORSTATIC: {
        HDC hc = (HDC)wParam;
        SetTextColor(hc, CLR_TEXT_DIM);
        SetBkColor  (hc, CLR_CELL_BG);
        static HBRUSH sb = NULL;
        if (!sb) sb = CreateSolidBrush(CLR_CELL_BG);
        return (LRESULT)sb;
    }
    case WM_CTLCOLOREDIT: {
        HDC hc = (HDC)wParam;
        SetTextColor(hc, CLR_TEXT);
        SetBkColor  (hc, RGB(20, 22, 32));
        static HBRUSH eb = NULL;
        if (!eb) eb = CreateSolidBrush(RGB(20, 22, 32));
        return (LRESULT)eb;
    }
    case WM_CTLCOLORBTN:
        return (LRESULT)GetStockObject(NULL_BRUSH);

    case WM_DESTROY:
        if (g_fntSmall)  DeleteObject(g_fntSmall);
        if (g_fntNormal) DeleteObject(g_fntNormal);
        if (g_fntBold)   DeleteObject(g_fntBold);
        if (g_fntBig)    DeleteObject(g_fntBig);
        if (g_fntHuge)   DeleteObject(g_fntHuge);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/* ================================================================== */
/*  WinMain                                                             */
/* ================================================================== */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance; (void)lpCmdLine;

    g_panel = panel_init();
    if (!g_panel) return 1;

    WNDCLASSA wc     = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "AvioniquePanelClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style         = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Echec RegisterClass", "Erreur", MB_ICONERROR);
        return 1;
    }

    RECT wr = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowA(
        "AvioniquePanelClass", "Panneau usager",
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
