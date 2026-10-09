// calendar-preview: gera um PNG do calendário SEM precisar do Hyprland.
//
// Usa exatamente o mesmo código de renderização do plugin, então o que você
// vê aqui é o que aparece no desktop. Com --screen WxH, simula também a
// posição (anchor/offset) numa tela do tamanho informado.

#include "calendar_core.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace hcal;

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a]))
        a++;
    while (b > a && std::isspace((unsigned char)s[b - 1]))
        b--;
    return s.substr(a, b - a);
}

static void usage() {
    std::puts(
        "Uso: calendar-preview [opções]\n"
        "\n"
        "  -c, --config ARQ      arquivo de configuração (linhas 'chave = valor')\n"
        "  -o, --output ARQ      PNG de saída (padrão: calendar.png)\n"
        "      --date AAAA-MM-DD mês/dia a exibir (padrão: hoje); o dia é marcado como 'hoje'\n"
        "      --scale N         escala do monitor (padrão: 1)\n"
        "      --screen LxA      simula uma tela (ex.: 1920x1080) e aplica anchor/offset\n"
        "      --wallpaper PNG   imagem de fundo para --screen (padrão: cor sólida)\n"
        "      --set chave=valor sobrescreve uma opção (pode repetir)\n"
        "  -h, --help            esta ajuda\n"
        "\n"
        "As chaves são as mesmas de plugin:calendar:<chave>, ex.: title_font, week_start.\n"
        "No arquivo, comentários começam com '# ' (um '#' seguido de espaço); '#RRGGBB' é cor.");
}

static bool loadConfigFile(Config& cfg, const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "erro: não consegui abrir %s\n", path.c_str());
        return false;
    }
    std::string line;
    int         n = 0;
    while (std::getline(f, line)) {
        n++;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0)
            continue;
        // permite colar o bloco `plugin { calendar { ... } }` do hyprland.conf
        if (line == "}" || line.back() == '{')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            std::fprintf(stderr, "%s:%d: linha ignorada (sem '=')\n", path.c_str(), n);
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string val = line.substr(eq + 1);
        // comentário no fim da linha: um '#' precedido E seguido de espaço
        // ("#RRGGBB" continua sendo cor)
        for (size_t i = 1; i < val.size(); i++) {
            if (val[i] == '#' && std::isspace((unsigned char)val[i - 1]) && (i + 1 >= val.size() || std::isspace((unsigned char)val[i + 1]))) {
                val = val.substr(0, i);
                break;
            }
        }
        val = trim(val);
        for (const char* prefix : {"plugin:calendar:", "calendar:"})
            if (key.rfind(prefix, 0) == 0)
                key = key.substr(std::strlen(prefix));

        std::string err;
        if (!setOption(cfg, key, val, err))
            std::fprintf(stderr, "%s:%d: %s\n", path.c_str(), n, err.c_str());
    }
    return true;
}

int main(int argc, char** argv) {
    Config                                      cfg;
    std::string                                 cfgFile, out = "calendar.png", wallpaper;
    std::vector<std::pair<std::string, std::string>> sets;
    double                                      scale = 1.0;
    int                                         screenW = 0, screenH = 0;
    int                                         year, month, day;
    today(year, month, day);

    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto              next = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "erro: %s precisa de um valor\n", a.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (a == "-c" || a == "--config")
            cfgFile = next();
        else if (a == "-o" || a == "--output")
            out = next();
        else if (a == "--scale")
            scale = std::atof(next());
        else if (a == "--wallpaper")
            wallpaper = next();
        else if (a == "--screen") {
            if (std::sscanf(next(), "%dx%d", &screenW, &screenH) != 2 || screenW <= 0 || screenH <= 0) {
                std::fprintf(stderr, "erro: --screen espera LARGURAxALTURA, ex.: 1920x1080\n");
                return 2;
            }
        } else if (a == "--date") {
            if (std::sscanf(next(), "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || month > 12 || day < 1 || day > 31) {
                std::fprintf(stderr, "erro: --date espera AAAA-MM-DD\n");
                return 2;
            }
        } else if (a == "--set") {
            const std::string kv = next();
            const size_t      eq = kv.find('=');
            if (eq == std::string::npos) {
                std::fprintf(stderr, "erro: --set espera chave=valor\n");
                return 2;
            }
            sets.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
        } else {
            std::fprintf(stderr, "opção desconhecida: %s\n\n", a.c_str());
            usage();
            return 2;
        }
    }

    if (!cfgFile.empty() && !loadConfigFile(cfg, cfgFile))
        return 1;
    for (auto& [k, v] : sets) {
        std::string err;
        if (!setOption(cfg, k, v, err)) {
            std::fprintf(stderr, "erro: %s\n", err.c_str());
            return 1;
        }
    }

    Rendered r = render(cfg, year, month, day, scale);
    if (!r.surface) {
        std::fprintf(stderr, "erro: falha ao renderizar\n");
        return 1;
    }

    cairo_surface_t* result = r.surface;

    if (screenW > 0) {
        const int pw = (int)(screenW * scale), ph = (int)(screenH * scale);
        result          = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
        cairo_t* cr     = cairo_create(result);

        // "wallpaper"
        bool drewWall = false;
        if (!wallpaper.empty()) {
            cairo_surface_t* w = cairo_image_surface_create_from_png(wallpaper.c_str());
            if (cairo_surface_status(w) == CAIRO_STATUS_SUCCESS) {
                const double ww = cairo_image_surface_get_width(w), wh = cairo_image_surface_get_height(w);
                const double k = std::max(pw / ww, ph / wh); // "cover"
                cairo_save(cr);
                cairo_translate(cr, (pw - ww * k) / 2, (ph - wh * k) / 2);
                cairo_scale(cr, k, k);
                cairo_set_source_surface(cr, w, 0, 0);
                cairo_paint(cr);
                cairo_restore(cr);
                drewWall = true;
            } else
                std::fprintf(stderr, "aviso: não consegui ler o wallpaper '%s' (só PNG); usando cor sólida\n", wallpaper.c_str());
            cairo_surface_destroy(w);
        }
        if (!drewWall) {
            cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, pw, ph);
            cairo_pattern_add_color_stop_rgb(g, 0, 0.17, 0.24, 0.33);
            cairo_pattern_add_color_stop_rgb(g, 1, 0.33, 0.24, 0.38);
            cairo_set_source(cr, g);
            cairo_paint(cr);
            cairo_pattern_destroy(g);
        }

        double x = 0, y = 0;
        place(cfg.anchor, screenW, screenH, r.logicalW, r.logicalH, cfg.offsetX, cfg.offsetY, x, y);
        cairo_set_source_surface(cr, r.surface, std::round(x * scale), std::round(y * scale));
        cairo_paint(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(r.surface);
    }

    if (cairo_surface_write_to_png(result, out.c_str()) != CAIRO_STATUS_SUCCESS) {
        std::fprintf(stderr, "erro: não consegui gravar %s\n", out.c_str());
        return 1;
    }
    std::printf("ok: %s (%dx%d px)\n", out.c_str(), cairo_image_surface_get_width(result), cairo_image_surface_get_height(result));
    cairo_surface_destroy(result);
    return 0;
}
