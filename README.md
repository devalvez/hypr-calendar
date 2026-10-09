# hypr-calendar

Plugin do Hyprland que desenha um calendário mensal no desktop, **entre o
wallpaper e as janelas**, no estilo minimalista da imagem de referência
(título grande, ano pequeno à direita, linha fina, dias da semana e números).

![preview.png](https://raw.githubusercontent.com/devalvez/hypr-calendar/refs/heads/main/preview.png)

Tudo é configurável: fontes, tamanhos e cores de cada elemento (título, ano,
dias da semana, números), idioma, nomes curtos/longos, primeiro dia da semana
e posição na tela.

<img src="https://github.com/devalvez/hypr-calendar/blob/main/examples.png" width="500px" alt="examples.png" />



## Testar sem instalar (sem Hyprland)

O desenho fica num núcleo separado (Cairo + Pango). O `calendar-preview` usa
exatamente o mesmo código do plugin e gera um PNG, então o que você vê aqui é
o que o plugin desenha.

```bash
# dependências: g++, pkg-config e as libs de desenvolvimento do pango/cairo
#   Arch:    sudo pacman -S pango cairo
#   Debian:  sudo apt install g++ pkg-config libpango1.0-dev libcairo2-dev
#   Fedora:  sudo dnf install gcc-c++ pkgconf pango-devel cairo-devel

make test                      # gera build/exemplo.png (agosto de 2026)
```

Experimente suas próprias opções:

```bash
# edite examples/preview.conf e gere de novo
build/calendar-preview -c examples/preview.conf -o meu.png

# sobrescrever opções na linha de comando
build/calendar-preview -c examples/preview.conf \
    --set language=pt_BR --set weekday_format=long --set width=560 -o pt.png

# simular a posição numa tela 1920x1080 (com wallpaper opcional, só PNG)
build/calendar-preview -c examples/preview.conf --screen 1920x1080 \
    --wallpaper ~/Imagens/wall.png --set anchor=bottom_left -o tela.png

# outro mês, em tela HiDPI
build/calendar-preview -c examples/preview.conf --date 2026-12-25 --scale 2 -o natal.png
```

As chaves do arquivo são as mesmas do plugin (`plugin:calendar:<chave>`).
Você pode até colar o bloco `plugin { calendar { ... } }` do `hyprland.conf`
no arquivo do preview.

## Instalar o plugin

Plugins do Hyprland são compilados contra **a mesma versão** do Hyprland que
está rodando. Precisa dos headers (`hyprland` no Arch já traz; em outras
distros, `hyprland-devel` ou `make installheaders` do código-fonte).

**Com o hyprpm:**

```bash
hyprpm add https://github.com/devalvez/hypr-calendar
hyprpm enable hypr-calendar
```

**Manual:**

```bash
make plugin                      # gera build/hypr-calendar.so
# (ou: cmake -B build && cmake --build build)
hyprctl plugin load $PWD/build/hypr-calendar.so
```

Para carregar sempre, veja `examples/hyprland.lua` (Hyprland com config em Lua,
`hl.plugin.load(...)`) ou `examples/hyprland.conf` (sintaxe clássica,
`plugin = ...`).

## Opções (`plugin:calendar:<nome>`)

Cores aceitam a sintaxe do Hyprland: `rgb(112233)`, `rgba(112233ee)`,
`0xAARRGGBB`. Tamanhos são em px lógicos.

| Opção | Padrão | Descrição |
|---|---|---|
| `enabled` | `true` | Mostra/oculta o calendário |
| `monitor` | vazio | Nomes de monitores separados por vírgula; vazio = todos |
| `anchor` | `top_right` | `top_left`, `top`, `top_right`, `left`, `center`, `right`, `bottom_left`, `bottom`, `bottom_right` |
| `offset_x`, `offset_y` | `40`, `40` | Margens a partir da âncora (para dentro da tela; em `center`/`top`/`bottom` etc. o eixo livre é deslocado) |
| `width` | `300` | Largura do calendário (a altura é automática) |
| `padding` | `26` | Margem interna |
| `row_height` | `0` | Altura de cada semana (0 = automático) |
| `rounding` | `0` | Raio dos cantos do fundo |
| `language` | `en` | `en`, `pt`, `es`, `fr`, `de`, `it` (aceita `pt_BR`, `en_US`...) |
| `week_start` | `sunday` | `sunday` ou `monday` |
| `weekday_format` | `short` | `short` (Sun) ou `long` (Sunday) |
| `title_font`, `title_size`, `title_color`, `title_case` | `Sans`, `56`, `#111111`, `lower` | Nome do mês. `title_case`: `lower`, `upper`, `capitalize` |
| `year_font`, `year_size`, `year_color`, `year_offset_y` | `Sans`, `15`, `#222222`, `0` | Ano |
| `weekday_font`, `weekday_size`, `weekday_color`, `weekday_case` | `Sans`, `12`, `#222222`, `upper` | Títulos dos dias da semana |
| `day_font`, `day_size`, `day_color` | `Sans`, `15`, `#222222` | Números dos dias |
| `today_style` | `none` | Destaque do dia atual: `none` (sem destaque) ou `badge` |
| `today_badge_shape` | `circle` | Formato do badge: `circle`, `rounded` ou `square` |
| `today_badge_color` | `#111111` | Cor do badge |
| `today_badge_text_color` | `#ffffff` | Cor do número dentro do badge |
| `today_badge_size` | `0` | Tamanho do badge em px (0 = automático, ajustado à célula) |
| `bg_color` | `#ffffff` | Cor do plano de fundo |
| `bg_opacity` | `0.93` | Transparência do fundo: `0.0` (invisível) a `1.0` (opaco). Multiplica o alpha da própria cor, se ela tiver (`rgba(...)`) |
| `line_color`, `line_width` | `#d9d9d9`, `1` | Linha sob o título (`line_width = 0` remove) |


### Example de Configuração (hyprland.lua)
```lua
hl.on("config.reloaded", function()
	hl.config({
        plugin = {
            calendar = {
                enabled = true,
                monitor = "",
                anchor = "bottom_right",
                offset_x = 40,
                offset_y = 40,
                width = 300,
                padding = 26,
                row_height = 0,
                rounding = 0,
                language = "en",
                week_start = "sunday",
                weekday_format = "short",
                title_font = "MagmaWave Caps",
                title_size = 56,
                title_color = "#ffffff",
                title_case = "lower",
                year_font = "Sans",
                year_size = 15,
                year_color = "#CCCCCC",
                year_offset_y = 0,
                weekday_font = "Comfortaa",
                weekday_size = 12,
                weekday_color = "#CCCCCC",
                weekday_case = "upper",
                day_font = "Comfortaa",
                day_size = 15,
                day_color = "#666666",
                today_style = "badge",
                today_badge_shape = "square",
                today_badge_color = "#DDDDDD",
                today_badge_text_color = "#18181b",
                today_badge_size = 0,
                bg_color = "#ffffff",
                bg_opacity = 0,
                line_color = "#d9d9d9",
                line_width = 1
            }
	    },
    })
end)
```

### Fontes

As opções `*_font` aceitam qualquer descrição do Pango: `Allura`,
`Inter Bold`, `JetBrains Mono Italic`... O visual da imagem de referência usa
uma fonte cursiva no título; instale uma (ex.: *Allura*, *Great Vibes*,
*Sacramento*) e use em `title_font`. Veja o que você tem com `fc-list : family`.

### Dias da semana por extenso

Com `weekday_format = long` os nomes são mais largos que a coluna. O texto
encolhe automaticamente para caber, então aumente o `width` (em torno de
`520`–`640` para português/inglês) ou reduza `weekday_size` para manter a
leitura confortável.

## Como funciona

- `src/calendar_core.*`: desenha o calendário (Cairo/Pango), idiomas,
  layout, posicionamento. Não depende do Hyprland.
- `src/preview.cpp`: ferramenta de linha de comando que gera PNG.
- `src/main.cpp`: plugin. No estágio `RENDER_POST_WALLPAPER` converte o
  desenho numa textura (reaproveitada entre frames; refeita só quando muda a
  config, o dia ou a escala do monitor) e a adiciona ao render pass.

## Limitações conhecidas

- A transparência afeta só o fundo; o texto e o badge mantêm a própria cor/alpha.
- O calendário é só visual: não captura cliques.
- Idiomas embutidos: en, pt, es, fr, de, it. Outros caem para inglês.
- A API de plugins do Hyprland muda entre versões. `src/main.cpp` foi escrito
  contra o 0.56; trechos dependentes da API estão marcados com `[API]`.
- Não reserva espaço de barras (waybar etc.): ajuste `offset_y`/`offset_x`.
