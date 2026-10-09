#include "calendar_core.hpp"

#include <pango/pangocairo.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

namespace hcal {

namespace {

// ---------------------------------------------------------------------------
// Idiomas
// ---------------------------------------------------------------------------

struct Lang {
    const char*                code;
    std::array<const char*, 12> months;
    std::array<const char*, 7>  longDays;  // começando no domingo
    std::array<const char*, 7>  shortDays; // começando no domingo
};

const Lang LANGS[] = {
    {"en",
     {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"},
     {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
     {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}},
    {"pt",
     {"Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"},
     {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"},
     {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"}},
    {"es",
     {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"},
     {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"},
     {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"}},
    {"fr",
     {"Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre", "Décembre"},
     {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"},
     {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"}},
    {"de",
     {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"},
     {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"},
     {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"}},
    {"it",
     {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno", "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"},
     {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"},
     {"Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab"}},
};

std::string asciiLower(std::string s) {
    for (auto& ch : s)
        ch = (char)std::tolower((unsigned char)ch);
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a]))
        a++;
    while (b > a && std::isspace((unsigned char)s[b - 1]))
        b--;
    return s.substr(a, b - a);
}

const Lang& findLang(const std::string& code) {
    const std::string c = asciiLower(trim(code));
    for (const auto& l : LANGS) {
        if (c.rfind(l.code, 0) == 0) // "pt_BR", "pt-br", "pt" -> pt
            return l;
    }
    return LANGS[0];
}

std::string applyCase(const std::string& s, const std::string& mode) {
    const std::string m = asciiLower(mode);
    gchar*            g = nullptr;
    if (m == "lower")
        g = g_utf8_strdown(s.c_str(), -1);
    else if (m == "upper")
        g = g_utf8_strup(s.c_str(), -1);
    else if (m == "capitalize") {
        gchar* low = g_utf8_strdown(s.c_str(), -1);
        if (low && *low) {
            gchar* next  = g_utf8_next_char(low);
            gchar* first = g_utf8_strup(low, (gssize)(next - low));
            std::string out = std::string(first ? first : "") + next;
            g_free(first);
            g_free(low);
            return out;
        }
        return s;
    } else
        return s;

    std::string out = g ? g : s;
    g_free(g);
    return out;
}

// ---------------------------------------------------------------------------
// Datas
// ---------------------------------------------------------------------------

int dayOfWeek(int y, int m, int d) { // 0 = domingo
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

int daysInMonth(int y, int m) {
    static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
        return 29;
    return dm[m - 1];
}

// ---------------------------------------------------------------------------
// Texto (Pango)
// ---------------------------------------------------------------------------

void setColor(cairo_t* cr, Color c) {
    cairo_set_source_rgba(cr, ((c >> 16) & 0xFF) / 255.0, ((c >> 8) & 0xFF) / 255.0, (c & 0xFF) / 255.0, ((c >> 24) & 0xFF) / 255.0);
}

class Text {
  public:
    Text(cairo_t* cr, const std::string& font, double px, const std::string& s) {
        m_layout = pango_cairo_create_layout(cr);
        PangoFontDescription* fd = pango_font_description_from_string(font.empty() ? "Sans" : font.c_str());
        pango_font_description_set_absolute_size(fd, std::max(1.0, px) * PANGO_SCALE);
        pango_layout_set_font_description(m_layout, fd);
        pango_font_description_free(fd);
        pango_layout_set_text(m_layout, s.c_str(), -1);
        pango_layout_get_extents(m_layout, &m_ink, &m_logical);
    }
    ~Text() {
        if (m_layout)
            g_object_unref(m_layout);
    }
    Text(const Text&)            = delete;
    Text& operator=(const Text&) = delete;

    double w() const { return m_logical.width / (double)PANGO_SCALE; }
    double h() const { return m_logical.height / (double)PANGO_SCALE; }
    // centro vertical da tinta, relativo ao topo do layout
    double inkCenterY() const { return (m_ink.y + m_ink.height / 2.0) / PANGO_SCALE; }

    void draw(cairo_t* cr, double x, double y, Color c) const {
        setColor(cr, c);
        cairo_move_to(cr, x, y);
        pango_cairo_show_layout(cr, m_layout);
    }

  private:
    PangoLayout*   m_layout = nullptr;
    PangoRectangle m_ink{}, m_logical{};
};

// ---------------------------------------------------------------------------
// Medidas
// ---------------------------------------------------------------------------

struct Metrics {
    double s = 1, W = 0, H = 0, pad = 0, inner = 0, gap = 0;

    std::string titleStr, yearStr;
    double      titleSize = 0, yearSize = 0, weekdaySize = 0, daySize = 0;
    double      titleH = 0, yearH = 0, headerH = 0;

    std::vector<std::string> weekdayLabels;
    double                   weekdayH = 0;

    double lineY = 0, lineH = 0, weekdayY = 0, gridY = 0, rowH = 0, cellW = 0;
    int    rows = 0, firstCol = 0, dim = 0;
};

Metrics measure(cairo_t* cr, const Config& c, int year, int month, double s) {
    Metrics m;
    m.s     = s;
    m.W     = std::round(std::max(50.0, c.width) * s);
    m.pad   = c.padding * s;
    m.inner = std::max(10.0, m.W - 2 * m.pad);
    m.gap   = c.padding * 0.55 * s;

    const Lang& L = findLang(c.language);

    m.titleStr = applyCase(L.months[month - 1], c.titleCase);
    m.yearStr  = std::to_string(year);

    // ano
    m.yearSize = c.yearSize * s;
    double yearW;
    {
        Text t(cr, c.yearFont, m.yearSize, m.yearStr);
        yearW   = t.w();
        m.yearH = t.h();
    }

    // título: encolhe se não couber ao lado do ano
    m.titleSize = c.titleSize * s;
    {
        Text       t(cr, c.titleFont, m.titleSize, m.titleStr);
        const double avail = m.inner - yearW - 10 * s;
        if (t.w() > avail && avail > 0) {
            m.titleSize *= avail / t.w();
            Text t2(cr, c.titleFont, m.titleSize, m.titleStr);
            m.titleH = t2.h();
        } else
            m.titleH = t.h();
    }
    m.headerH = std::max(m.titleH, m.yearH);

    // dias da semana: encolhe se o nome mais largo não couber na coluna
    m.cellW = m.inner / 7.0;
    for (int i = 0; i < 7; i++) {
        const int idx = (i + (c.weekStartMon ? 1 : 0)) % 7;
        m.weekdayLabels.push_back(applyCase(c.weekdayLong ? L.longDays[idx] : L.shortDays[idx], c.weekdayCase));
    }
    m.weekdaySize = c.weekdaySize * s;
    {
        double maxW = 0;
        for (const auto& lbl : m.weekdayLabels) {
            Text t(cr, c.weekdayFont, m.weekdaySize, lbl);
            maxW = std::max(maxW, t.w());
        }
        const double limit = m.cellW * 0.94;
        if (maxW > limit && maxW > 0)
            m.weekdaySize *= limit / maxW;

        Text t(cr, c.weekdayFont, m.weekdaySize, "Ag");
        m.weekdayH = t.h();
    }

    m.daySize = c.daySize * s;
    m.rowH    = (c.rowHeight > 0 ? c.rowHeight : c.daySize * 2.2) * s;

    m.dim      = daysInMonth(year, month);
    m.firstCol = (dayOfWeek(year, month, 1) - (c.weekStartMon ? 1 : 0) + 7) % 7;
    m.rows     = (m.firstCol + m.dim + 6) / 7;

    // posições verticais
    double y = m.pad + m.headerH + m.gap;
    m.lineY  = y;
    m.lineH  = c.lineWidth > 0 ? std::max(1.0, std::round(c.lineWidth * s)) : 0;
    y += m.lineH + m.gap;
    m.weekdayY = y;
    y += m.weekdayH + m.gap * 0.8;
    m.gridY = y;
    y += m.rows * m.rowH + m.pad;
    m.H = std::ceil(y);

    return m;
}

void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r) {
    r = std::min(r, std::min(w, h) / 2.0);
    if (r <= 0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

// multiplica o alpha da cor por `opacity` (0..1)
Color withOpacity(Color c, double opacity) {
    const double a = ((c >> 24) & 0xFF) * std::clamp(opacity, 0.0, 1.0);
    return (c & 0x00FFFFFF) | ((uint32_t)std::lround(a) << 24);
}

void paint(cairo_t* cr, const Config& c, const Metrics& m, int today) {
    // fundo
    roundedRect(cr, 0, 0, m.W, m.H, c.rounding * m.s);
    setColor(cr, withOpacity(c.bgColor, c.bgOpacity));
    cairo_fill(cr);

    // título (esquerda) e ano (direita), alinhados pela base da caixa do cabeçalho
    {
        Text t(cr, c.titleFont, m.titleSize, m.titleStr);
        t.draw(cr, m.pad, m.pad + m.headerH - t.h(), c.titleColor);
    }
    {
        Text t(cr, c.yearFont, m.yearSize, m.yearStr);
        t.draw(cr, m.W - m.pad - t.w(), m.pad + m.headerH - t.h() + c.yearOffsetY * m.s, c.yearColor);
    }

    // linha
    if (m.lineH > 0) {
        setColor(cr, c.lineColor);
        cairo_rectangle(cr, m.pad, m.lineY, m.inner, m.lineH);
        cairo_fill(cr);
    }

    // dias da semana
    for (int i = 0; i < 7; i++) {
        Text         t(cr, c.weekdayFont, m.weekdaySize, m.weekdayLabels[i]);
        const double cx = m.pad + (i + 0.5) * m.cellW;
        t.draw(cr, cx - t.w() / 2, m.weekdayY, c.weekdayColor);
    }

    // números: centraliza verticalmente pela tinta dos dígitos
    double digitCenter;
    {
        Text t(cr, c.dayFont, m.daySize, "0123456789");
        digitCenter = t.inkCenterY();
    }
    for (int d = 1; d <= m.dim; d++) {
        const int    idx = m.firstCol + d - 1;
        const double cx  = m.pad + (idx % 7 + 0.5) * m.cellW;
        const double cy  = m.gridY + (idx / 7 + 0.5) * m.rowH;

        Color col = c.dayColor;
        if (d == today && asciiLower(c.todayStyle) == "badge") {
            const std::string shape = asciiLower(c.todayBadgeShape);
            // tamanho automático: cabe na célula; ou o valor pedido (px lógicos)
            const double size = c.todayBadgeSize > 0 ? c.todayBadgeSize * m.s : std::min(m.cellW * 0.92, m.rowH * 0.84);

            setColor(cr, c.todayBadgeColor);
            if (shape == "square")
                cairo_rectangle(cr, cx - size / 2, cy - size / 2, size, size);
            else if (shape == "rounded")
                roundedRect(cr, cx - size / 2, cy - size / 2, size, size, size * 0.28);
            else
                cairo_arc(cr, cx, cy, size / 2, 0, 2 * M_PI);
            cairo_fill(cr);

            col = c.todayBadgeTextColor;
        }

        Text t(cr, c.dayFont, m.daySize, std::to_string(d));
        t.draw(cr, cx - t.w() / 2, cy - digitCenter, col);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

Rendered render(const Config& cfg, int year, int month, int today, double scale) {
    month = std::clamp(month, 1, 12);
    scale = std::clamp(scale, 0.25, 8.0);

    Metrics m;
    {
        cairo_surface_t* dummy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        cairo_t*         cr    = cairo_create(dummy);
        m                      = measure(cr, cfg, year, month, scale);
        cairo_destroy(cr);
        cairo_surface_destroy(dummy);
    }

    Rendered r;
    r.pxW      = (int)m.W;
    r.pxH      = (int)m.H;
    r.logicalW = m.W / scale;
    r.logicalH = m.H / scale;
    r.surface  = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, r.pxW, r.pxH);

    cairo_t* cr = cairo_create(r.surface);
    paint(cr, cfg, m, today);
    cairo_destroy(cr);
    cairo_surface_flush(r.surface);
    return r;
}

void place(const std::string& anchorIn, double sw, double sh, double bw, double bh, double ox, double oy, double& x, double& y) {
    const std::string a = asciiLower(anchorIn);

    const bool left   = a == "top_left" || a == "left" || a == "bottom_left";
    const bool right  = a == "top_right" || a == "right" || a == "bottom_right";
    const bool top    = a == "top_left" || a == "top" || a == "top_right";
    const bool bottom = a == "bottom_left" || a == "bottom" || a == "bottom_right";

    if (left)
        x = ox;
    else if (right)
        x = sw - bw - ox;
    else
        x = (sw - bw) / 2.0 + ox;

    if (top)
        y = oy;
    else if (bottom)
        y = sh - bh - oy;
    else
        y = (sh - bh) / 2.0 + oy;
}

void today(int& year, int& month, int& day) {
    std::time_t t = std::time(nullptr);
    std::tm     tm{};
    localtime_r(&t, &tm);
    year  = tm.tm_year + 1900;
    month = tm.tm_mon + 1;
    day   = tm.tm_mday;
}

// ---------------------------------------------------------------------------
// Cores e opções
// ---------------------------------------------------------------------------

namespace {

bool parseHex(const std::string& h, uint32_t& v) {
    if (h.empty() || h.size() > 8)
        return false;
    for (char ch : h)
        if (!std::isxdigit((unsigned char)ch))
            return false;
    v = (uint32_t)std::strtoul(h.c_str(), nullptr, 16);
    return true;
}

std::vector<std::string> splitComma(const std::string& s) {
    std::vector<std::string> out;
    size_t                   start = 0;
    while (true) {
        size_t p = s.find(',', start);
        out.push_back(trim(s.substr(start, p == std::string::npos ? std::string::npos : p - start)));
        if (p == std::string::npos)
            break;
        start = p + 1;
    }
    return out;
}

Color pack(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return (std::min(a, 255u) << 24) | (std::min(r, 255u) << 16) | (std::min(g, 255u) << 8) | std::min(b, 255u);
}

} // namespace

bool parseColor(const std::string& in, Color& out) {
    const std::string s = asciiLower(trim(in));
    uint32_t          v = 0;

    auto inner = [&](const char* prefix) -> std::string {
        const std::string p = prefix;
        if (s.rfind(p, 0) != 0 || s.back() != ')')
            return "\x01";
        return trim(s.substr(p.size(), s.size() - p.size() - 1));
    };

    if (s.rfind("rgba(", 0) == 0) {
        const std::string in2 = inner("rgba(");
        if (in2 == "\x01")
            return false;
        if (in2.find(',') != std::string::npos) {
            auto p = splitComma(in2);
            if (p.size() != 4)
                return false;
            out = pack((uint32_t)std::lround(std::clamp(std::atof(p[3].c_str()), 0.0, 1.0) * 255), (uint32_t)std::atoi(p[0].c_str()), (uint32_t)std::atoi(p[1].c_str()),
                       (uint32_t)std::atoi(p[2].c_str()));
            return true;
        }
        if (in2.size() != 8 || !parseHex(in2, v))
            return false;
        out = ((v & 0xFF) << 24) | (v >> 8); // RRGGBBAA -> AARRGGBB
        return true;
    }
    if (s.rfind("rgb(", 0) == 0) {
        const std::string in2 = inner("rgb(");
        if (in2 == "\x01")
            return false;
        if (in2.find(',') != std::string::npos) {
            auto p = splitComma(in2);
            if (p.size() != 3)
                return false;
            out = pack(255, (uint32_t)std::atoi(p[0].c_str()), (uint32_t)std::atoi(p[1].c_str()), (uint32_t)std::atoi(p[2].c_str()));
            return true;
        }
        if (in2.size() != 6 || !parseHex(in2, v))
            return false;
        out = 0xFF000000 | v;
        return true;
    }
    if (s.rfind("0x", 0) == 0) {
        const std::string h = s.substr(2);
        if (!parseHex(h, v))
            return false;
        out = h.size() <= 6 ? (0xFF000000 | v) : v;
        return true;
    }
    if (!s.empty() && s[0] == '#') {
        const std::string h = s.substr(1);
        if (h.size() == 6 && parseHex(h, v)) {
            out = 0xFF000000 | v;
            return true;
        }
        if (h.size() == 8 && parseHex(h, v)) {
            out = ((v & 0xFF) << 24) | (v >> 8);
            return true;
        }
    }
    return false;
}

bool setOption(Config& c, const std::string& keyIn, const std::string& valueIn, std::string& err) {
    const std::string k = asciiLower(trim(keyIn));
    const std::string v = trim(valueIn);

    auto asNum = [&](double& dst) {
        char* end = nullptr;
        double d  = std::strtod(v.c_str(), &end);
        if (end == v.c_str() || *end != '\0') {
            err = "valor numérico inválido para '" + k + "': " + v;
            return false;
        }
        dst = d;
        return true;
    };
    auto asInt = [&](int& dst) {
        double d;
        if (!asNum(d))
            return false;
        dst = (int)std::lround(d);
        return true;
    };
    auto asColor = [&](Color& dst) {
        if (!parseColor(v, dst)) {
            err = "cor inválida para '" + k + "': " + v;
            return false;
        }
        return true;
    };
    auto asStr = [&](std::string& dst) {
        dst = v;
        return true;
    };

    if (k == "anchor") {
        static const char* ok[] = {"top_left", "top", "top_right", "left", "center", "right", "bottom_left", "bottom", "bottom_right"};
        const std::string a = asciiLower(v);
        for (auto o : ok)
            if (a == o) {
                c.anchor = a;
                return true;
            }
        err = "anchor inválido: " + v + " (use top_left, top, top_right, left, center, right, bottom_left, bottom, bottom_right)";
        return false;
    }
    if (k == "offset_x") return asInt(c.offsetX);
    if (k == "offset_y") return asInt(c.offsetY);
    if (k == "width") return asNum(c.width);
    if (k == "padding") return asNum(c.padding);
    if (k == "row_height") return asNum(c.rowHeight);
    if (k == "rounding") return asNum(c.rounding);

    if (k == "language") return asStr(c.language);
    if (k == "week_start") {
        const std::string a = asciiLower(v);
        if (a == "sunday" || a == "sun" || a == "0") { c.weekStartMon = false; return true; }
        if (a == "monday" || a == "mon" || a == "1") { c.weekStartMon = true; return true; }
        err = "week_start deve ser sunday ou monday";
        return false;
    }
    if (k == "weekday_format") {
        const std::string a = asciiLower(v);
        if (a == "short") { c.weekdayLong = false; return true; }
        if (a == "long") { c.weekdayLong = true; return true; }
        err = "weekday_format deve ser short ou long";
        return false;
    }

    if (k == "title_font") return asStr(c.titleFont);
    if (k == "title_size") return asNum(c.titleSize);
    if (k == "title_color") return asColor(c.titleColor);
    if (k == "title_case") return asStr(c.titleCase);

    if (k == "year_font") return asStr(c.yearFont);
    if (k == "year_size") return asNum(c.yearSize);
    if (k == "year_color") return asColor(c.yearColor);
    if (k == "year_offset_y") return asNum(c.yearOffsetY);

    if (k == "weekday_font") return asStr(c.weekdayFont);
    if (k == "weekday_size") return asNum(c.weekdaySize);
    if (k == "weekday_color") return asColor(c.weekdayColor);
    if (k == "weekday_case") return asStr(c.weekdayCase);

    if (k == "day_font") return asStr(c.dayFont);
    if (k == "day_size") return asNum(c.daySize);
    if (k == "day_color") return asColor(c.dayColor);

    if (k == "today_style") {
        const std::string a = asciiLower(v);
        if (a == "none" || a == "badge") { c.todayStyle = a; return true; }
        err = "today_style deve ser none ou badge";
        return false;
    }
    if (k == "today_badge_shape") {
        const std::string a = asciiLower(v);
        if (a == "circle" || a == "rounded" || a == "square") { c.todayBadgeShape = a; return true; }
        err = "today_badge_shape deve ser circle, rounded ou square";
        return false;
    }
    if (k == "today_badge_color") return asColor(c.todayBadgeColor);
    if (k == "today_badge_text_color") return asColor(c.todayBadgeTextColor);
    if (k == "today_badge_size") return asNum(c.todayBadgeSize);

    if (k == "bg_color") return asColor(c.bgColor);
    if (k == "bg_opacity") return asNum(c.bgOpacity);
    if (k == "line_color") return asColor(c.lineColor);
    if (k == "line_width") return asNum(c.lineWidth);

    err = "opção desconhecida: " + k;
    return false;
}

} // namespace hcal
