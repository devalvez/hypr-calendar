// hypr-calendar: núcleo de renderização (independente do Hyprland).
//
// Este arquivo é compartilhado entre o plugin (main.cpp) e a ferramenta de
// preview (preview.cpp). Tudo que aparece na tela é desenhado aqui com
// Cairo + Pango, então o PNG gerado pelo preview é exatamente o que o plugin
// desenha no desktop.
#pragma once

#include <cairo.h>

#include <cstdint>
#include <string>

namespace hcal {

// Cores sempre em 0xAARRGGBB (mesmo formato interno das cores do Hyprland).
using Color = uint32_t;

struct Config {
    // ---- posicionamento ------------------------------------------------
    // anchor: top_left, top, top_right, left, center, right,
    //         bottom_left, bottom, bottom_right
    std::string anchor  = "top_right";
    int         offsetX = 40; // margem horizontal (para dentro a partir da âncora)
    int         offsetY = 40; // margem vertical   (para dentro a partir da âncora)

    // ---- geometria (px lógicos) ----------------------------------------
    double width     = 300; // largura total do calendário
    double padding   = 26;  // margem interna
    double rowHeight = 0;   // altura de cada semana; 0 = automático (day_size * 2.2)
    double rounding  = 0;   // raio dos cantos do fundo

    // ---- idioma / semana -----------------------------------------------
    std::string language      = "en"; // en, pt, es, fr, de, it (aceita pt_BR, en_US...)
    bool        weekStartMon  = false; // false = domingo, true = segunda
    bool        weekdayLong   = false; // false = Sun, true = Sunday

    // ---- título (nome do mês) ------------------------------------------
    std::string titleFont  = "Sans";
    double      titleSize  = 56;
    Color       titleColor = 0xFF111111;
    std::string titleCase  = "lower"; // lower, upper, capitalize

    // ---- ano -----------------------------------------------------------
    std::string yearFont    = "Sans";
    double      yearSize    = 15;
    Color       yearColor   = 0xFF222222;
    double      yearOffsetY = 0; // ajuste fino vertical em px (positivo = para baixo)

    // ---- títulos dos dias da semana ------------------------------------
    std::string weekdayFont  = "Sans";
    double      weekdaySize  = 12;
    Color       weekdayColor = 0xFF222222;
    std::string weekdayCase  = "upper"; // lower, upper, capitalize

    // ---- números dos dias ----------------------------------------------
    std::string dayFont  = "Sans";
    double      daySize  = 15;
    Color       dayColor = 0xFF222222;

    // ---- destaque do dia atual -----------------------------------------
    std::string todayStyle          = "none";   // none | badge
    std::string todayBadgeShape     = "circle"; // circle | rounded | square
    Color       todayBadgeColor     = 0xFF111111; // cor do badge
    Color       todayBadgeTextColor = 0xFFFFFFFF; // cor do número dentro do badge
    double      todayBadgeSize      = 0;          // lado/diâmetro em px; 0 = automático

    // ---- fundo e linha separadora --------------------------------------
    Color  bgColor   = 0xFFFFFFFF; // cor do plano de fundo (o alpha da cor também vale)
    double bgOpacity = 0.93;       // transparência do fundo: 0.0 (invisível) a 1.0 (opaco)
    Color  lineColor = 0xFFD9D9D9;
    double lineWidth = 1; // 0 = sem linha
};

struct Rendered {
    cairo_surface_t* surface = nullptr; // ARGB32 pré-multiplicado; dono: o chamador
    int              pxW = 0, pxH = 0;  // tamanho em pixels físicos
    double           logicalW = 0, logicalH = 0;
};

// Desenha o calendário de `month` (1-12) / `year`. `today` (1-31) marca o dia
// atual quando o mês exibido é o mês corrente; 0 desativa o destaque.
// `scale` é a escala do monitor (1.0, 1.25, 2.0...).
Rendered render(const Config& cfg, int year, int month, int today, double scale);

// Calcula o canto superior esquerdo (px lógicos) para o calendário dentro de
// uma tela de screenW x screenH.
void place(const std::string& anchor, double screenW, double screenH, double boxW, double boxH, double offX, double offY, double& x, double& y);

// Data atual do sistema.
void today(int& year, int& month, int& day);

// Aceita: rgba(RRGGBBAA)  rgba(r,g,b,a)  rgb(RRGGBB)  rgb(r,g,b)
//         0xAARRGGBB  #RRGGBB  #RRGGBBAA   (mesma sintaxe do Hyprland)
bool parseColor(const std::string& s, Color& out);

// Define uma opção pelo nome (mesmos nomes de plugin:calendar:<nome>).
// Usado pelo arquivo de configuração do preview. Retorna false e preenche
// `err` quando o nome ou o valor são inválidos.
bool setOption(Config& cfg, const std::string& key, const std::string& value, std::string& err);

} // namespace hcal
