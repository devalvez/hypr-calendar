// hypr-calendar: plugin do Hyprland que desenha um calendário mensal no
// desktop, entre o wallpaper e as janelas.
//
// Toda a parte visual está em calendar_core.cpp (Cairo/Pango). Este arquivo
// só faz a "cola" com o Hyprland:
//   1. registra as opções  plugin:calendar:*
//   2. no estágio RENDER_POST_WALLPAPER, converte o calendário numa textura
//      e a adiciona ao render pass do monitor.
//
// Escrito contra a API do Hyprland 0.56 (EventBus, config V2, Render::*).
// A API de plugins muda bastante entre versões: se o build quebrar numa
// versão nova, os pontos a ajustar são os marcados com [API] abaixo.

#define WLR_USE_UNSTABLE

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "calendar_core.hpp"

inline HANDLE PHANDLE = nullptr;

// Não altere esta função.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

namespace {

using namespace Config::Values;

// ---------------------------------------------------------------------------
// Opções  (plugin:calendar:<nome>)
// ---------------------------------------------------------------------------

struct SOpts {
    SP<CBoolValue>   enabled;
    SP<CStringValue> monitor, anchor, language, weekStart, weekdayFormat;
    SP<CStringValue> titleFont, titleCase, yearFont, weekdayFont, weekdayCase, dayFont, todayStyle, todayBadgeShape;
    SP<CIntValue>    offsetX, offsetY;
    SP<CFloatValue>  width, padding, rowHeight, rounding, titleSize, yearSize, yearOffsetY, weekdaySize, daySize, lineWidth, todayBadgeSize, bgOpacity;
    SP<CColorValue>  titleColor, yearColor, weekdayColor, dayColor, todayBadgeColor, todayBadgeTextColor, bgColor, lineColor;
};

SOpts g_opts;

// [API] addConfigValueV2 + Config::Values::C*Value (0.5x). Em versões antigas
// era HyprlandAPI::addConfigValue(handle, nome, Hyprlang::INT{...}).
template <typename T, typename D>
SP<T> addOption(const char* name, const char* desc, D def) {
    auto v = makeShared<T>(name, desc, def);
    HyprlandAPI::addConfigValueV2(PHANDLE, v);
    return v;
}

void registerOptions() {
    const hcal::Config d; // os padrões vêm do núcleo, para ficarem sempre iguais

    auto& o = g_opts;
    // clang-format off
    o.enabled       = addOption<CBoolValue>  ("plugin:calendar:enabled",         "Mostrar o calendário", true);
    o.monitor       = addOption<CStringValue>("plugin:calendar:monitor",         "Monitores (nomes separados por vírgula). Vazio = todos", "");

    o.anchor        = addOption<CStringValue>("plugin:calendar:anchor",          "Âncora: top_left, top, top_right, left, center, right, bottom_left, bottom, bottom_right", d.anchor);
    o.offsetX       = addOption<CIntValue>   ("plugin:calendar:offset_x",        "Margem horizontal em px", (Config::INTEGER)d.offsetX);
    o.offsetY       = addOption<CIntValue>   ("plugin:calendar:offset_y",        "Margem vertical em px", (Config::INTEGER)d.offsetY);

    o.width         = addOption<CFloatValue> ("plugin:calendar:width",           "Largura do calendário em px", (Config::FLOAT)d.width);
    o.padding       = addOption<CFloatValue> ("plugin:calendar:padding",         "Margem interna em px", (Config::FLOAT)d.padding);
    o.rowHeight     = addOption<CFloatValue> ("plugin:calendar:row_height",      "Altura de cada semana em px (0 = automático)", (Config::FLOAT)d.rowHeight);
    o.rounding      = addOption<CFloatValue> ("plugin:calendar:rounding",        "Raio dos cantos do fundo em px", (Config::FLOAT)d.rounding);

    o.language      = addOption<CStringValue>("plugin:calendar:language",        "Idioma: en, pt, es, fr, de, it (aceita pt_BR, en_US...)", d.language);
    o.weekStart     = addOption<CStringValue>("plugin:calendar:week_start",      "Primeiro dia da semana: sunday ou monday", "sunday");
    o.weekdayFormat = addOption<CStringValue>("plugin:calendar:weekday_format",  "Dias da semana: short (Sun) ou long (Sunday)", "short");

    o.titleFont     = addOption<CStringValue>("plugin:calendar:title_font",      "Fonte do título (mês). Aceita descrição Pango, ex.: 'Allura'", d.titleFont);
    o.titleSize     = addOption<CFloatValue> ("plugin:calendar:title_size",      "Tamanho do título em px", (Config::FLOAT)d.titleSize);
    o.titleColor    = addOption<CColorValue> ("plugin:calendar:title_color",     "Cor do título", (Config::INTEGER)d.titleColor);
    o.titleCase     = addOption<CStringValue>("plugin:calendar:title_case",      "Caixa do título: lower, upper, capitalize", d.titleCase);

    o.yearFont      = addOption<CStringValue>("plugin:calendar:year_font",       "Fonte do ano", d.yearFont);
    o.yearSize      = addOption<CFloatValue> ("plugin:calendar:year_size",       "Tamanho do ano em px", (Config::FLOAT)d.yearSize);
    o.yearColor     = addOption<CColorValue> ("plugin:calendar:year_color",      "Cor do ano", (Config::INTEGER)d.yearColor);
    o.yearOffsetY   = addOption<CFloatValue> ("plugin:calendar:year_offset_y",   "Ajuste vertical do ano em px (positivo = para baixo)", (Config::FLOAT)d.yearOffsetY);

    o.weekdayFont   = addOption<CStringValue>("plugin:calendar:weekday_font",    "Fonte dos dias da semana", d.weekdayFont);
    o.weekdaySize   = addOption<CFloatValue> ("plugin:calendar:weekday_size",    "Tamanho dos dias da semana em px", (Config::FLOAT)d.weekdaySize);
    o.weekdayColor  = addOption<CColorValue> ("plugin:calendar:weekday_color",   "Cor dos dias da semana", (Config::INTEGER)d.weekdayColor);
    o.weekdayCase   = addOption<CStringValue>("plugin:calendar:weekday_case",    "Caixa dos dias da semana: lower, upper, capitalize", d.weekdayCase);

    o.dayFont       = addOption<CStringValue>("plugin:calendar:day_font",        "Fonte dos números dos dias", d.dayFont);
    o.daySize       = addOption<CFloatValue> ("plugin:calendar:day_size",        "Tamanho dos números em px", (Config::FLOAT)d.daySize);
    o.dayColor      = addOption<CColorValue> ("plugin:calendar:day_color",       "Cor dos números", (Config::INTEGER)d.dayColor);

    o.todayStyle          = addOption<CStringValue>("plugin:calendar:today_style",            "Destaque do dia atual: none ou badge", d.todayStyle);
    o.todayBadgeShape     = addOption<CStringValue>("plugin:calendar:today_badge_shape",      "Formato do badge: circle, rounded ou square", d.todayBadgeShape);
    o.todayBadgeColor     = addOption<CColorValue> ("plugin:calendar:today_badge_color",      "Cor do badge do dia atual", (Config::INTEGER)d.todayBadgeColor);
    o.todayBadgeTextColor = addOption<CColorValue> ("plugin:calendar:today_badge_text_color", "Cor do número dentro do badge", (Config::INTEGER)d.todayBadgeTextColor);
    o.todayBadgeSize      = addOption<CFloatValue> ("plugin:calendar:today_badge_size",       "Tamanho do badge em px (0 = automático)", (Config::FLOAT)d.todayBadgeSize);

    o.bgColor       = addOption<CColorValue> ("plugin:calendar:bg_color",        "Cor do plano de fundo do calendário", (Config::INTEGER)d.bgColor);
    o.bgOpacity     = addOption<CFloatValue> ("plugin:calendar:bg_opacity",      "Opacidade do fundo: 0.0 (transparente) a 1.0 (opaco)", (Config::FLOAT)d.bgOpacity);
    o.lineColor     = addOption<CColorValue> ("plugin:calendar:line_color",      "Cor da linha separadora", (Config::INTEGER)d.lineColor);
    o.lineWidth     = addOption<CFloatValue> ("plugin:calendar:line_width",      "Espessura da linha em px (0 = sem linha)", (Config::FLOAT)d.lineWidth);
    // clang-format on
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------

struct SCacheEntry {
    SP<Render::ITexture> tex;
    double               scale = 0;
    int                  year = 0, month = 0, day = 0;
    int                  pxW = 0, pxH = 0;
    double               logicalW = 0, logicalH = 0;
};

hcal::Config                             g_cfg;
std::string                              g_monitorFilter;
bool                                     g_enabled = true;
bool                                     g_dirty   = true;
std::unordered_map<std::string, SCacheEntry> g_cache; // por monitor

hcal::Config readConfig() {
    hcal::Config c;
    auto&        o = g_opts;
    std::string  err;

    // Opções de texto passam pelo mesmo validador do preview; valores
    // inválidos mantêm o padrão em vez de quebrar o desenho.
    auto str = [&](const char* key, const std::string& v) { hcal::setOption(c, key, v, err); };

    str("anchor", o.anchor->value());
    str("language", o.language->value());
    str("week_start", o.weekStart->value());
    str("weekday_format", o.weekdayFormat->value());
    str("title_font", o.titleFont->value());
    str("title_case", o.titleCase->value());
    str("year_font", o.yearFont->value());
    str("weekday_font", o.weekdayFont->value());
    str("weekday_case", o.weekdayCase->value());
    str("day_font", o.dayFont->value());
    str("today_style", o.todayStyle->value());
    str("today_badge_shape", o.todayBadgeShape->value());

    c.offsetX     = (int)o.offsetX->value();
    c.offsetY     = (int)o.offsetY->value();
    c.width       = o.width->value();
    c.padding     = o.padding->value();
    c.rowHeight   = o.rowHeight->value();
    c.rounding    = o.rounding->value();
    c.titleSize   = o.titleSize->value();
    c.yearSize    = o.yearSize->value();
    c.yearOffsetY = o.yearOffsetY->value();
    c.weekdaySize = o.weekdaySize->value();
    c.daySize     = o.daySize->value();
    c.lineWidth   = o.lineWidth->value();
    c.todayBadgeSize = o.todayBadgeSize->value();
    c.bgOpacity   = o.bgOpacity->value();

    c.titleColor   = (hcal::Color)o.titleColor->value();
    c.yearColor    = (hcal::Color)o.yearColor->value();
    c.weekdayColor = (hcal::Color)o.weekdayColor->value();
    c.dayColor     = (hcal::Color)o.dayColor->value();
    c.todayBadgeColor     = (hcal::Color)o.todayBadgeColor->value();
    c.todayBadgeTextColor = (hcal::Color)o.todayBadgeTextColor->value();
    c.bgColor      = (hcal::Color)o.bgColor->value();
    c.lineColor    = (hcal::Color)o.lineColor->value();
    return c;
}

bool monitorAllowed(const std::string& name) {
    if (g_monitorFilter.empty())
        return true;

    size_t start = 0;
    while (start <= g_monitorFilter.size()) {
        size_t end = g_monitorFilter.find(',', start);
        if (end == std::string::npos)
            end = g_monitorFilter.size();

        size_t a = start, b = end;
        while (a < b && g_monitorFilter[a] == ' ')
            a++;
        while (b > a && g_monitorFilter[b - 1] == ' ')
            b--;
        if (g_monitorFilter.compare(a, b - a, name) == 0)
            return true;
        start = end + 1;
    }
    return false;
}

void damageAllMonitors() {
    // [API] State::monitorState()->monitors() e g_pHyprRenderer->damageMonitor()
    for (auto& m : State::monitorState()->monitors())
        g_pHyprRenderer->damageMonitor(m);
}

// ---------------------------------------------------------------------------
// Renderização
// ---------------------------------------------------------------------------

// [API] Event::SRenderStageEvent: `context` só existe nos estágios com GL
// ativo; RENDER_POST_WALLPAPER tem contexto.
void onRenderStage(const Event::SRenderStageEvent& e) {
    if (e.stage != RENDER_POST_WALLPAPER || !e.context.has_value() || !e.monitor)
        return;

    if (g_dirty) {
        g_cfg           = readConfig();
        g_enabled       = g_opts.enabled->value();
        g_monitorFilter = g_opts.monitor->value();
        g_cache.clear();
        g_dirty = false;
    }

    if (!g_enabled)
        return;

    const auto MON = e.monitor;
    if (!monitorAllowed(MON->m_name))
        return;

    int year, month, day;
    hcal::today(year, month, day);
    const double scale = MON->m_scale;

    auto& ce = g_cache[MON->m_name];
    if (!ce.tex || !ce.tex->ok() || ce.scale != scale || ce.year != year || ce.month != month || ce.day != day) {
        hcal::Rendered r = hcal::render(g_cfg, year, month, day, scale);
        if (!r.surface)
            return;

        // [API] IHyprRenderer::createTexture(cairo_surface_t*)
        ce.tex = g_pHyprRenderer->createTexture(r.surface);
        cairo_surface_destroy(r.surface);

        ce.scale    = scale;
        ce.year     = year;
        ce.month    = month;
        ce.day      = day;
        ce.pxW      = r.pxW;
        ce.pxH      = r.pxH;
        ce.logicalW = r.logicalW;
        ce.logicalH = r.logicalH;

        // a virada do dia (ou da escala) precisa repintar a área
        g_pHyprRenderer->damageMonitor(MON);
    }

    if (!ce.tex)
        return;

    // posição em px lógicos, relativa ao canto do monitor
    double x = 0, y = 0;
    hcal::place(g_cfg.anchor, MON->m_size.x, MON->m_size.y, ce.logicalW, ce.logicalH, g_cfg.offsetX, g_cfg.offsetY, x, y);

    // [API] CTexPassElement: a caixa é em pixels FÍSICOS (o próprio elemento
    // divide pela escala do monitor para calcular o bounding box lógico).
    CTexPassElement::SRenderData data;
    data.tex = ce.tex;
    data.box = {std::round(x * scale), std::round(y * scale), (double)ce.pxW, (double)ce.pxH};
    data.a   = 1.F;

    e.context->get().m_pass.add(makeUnique<CTexPassElement>(std::move(data)));
}

} // namespace

// ---------------------------------------------------------------------------
// Ciclo de vida do plugin
// ---------------------------------------------------------------------------

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hypr-calendar] Falha ao iniciar: versão incompatível (headers != Hyprland em execução)", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hypr-calendar] version mismatch");
    }

    registerOptions();

    static auto P1 = Event::bus()->m_events.render.stage.listen([](const Event::SRenderStageEvent& e) { onRenderStage(e); });
    static auto P2 = Event::bus()->m_events.config.reloaded.listen([] {
        g_dirty = true;
        damageAllMonitors();
    });

    HyprlandAPI::reloadConfig();
    damageAllMonitors();

    return {"hypr-calendar", "Calendário mensal no desktop, por cima do wallpaper.", "hypr-calendar", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_cache.clear();
    damageAllMonitors();
}
