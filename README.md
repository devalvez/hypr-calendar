# hypr-calendar

Plugin do Hyprland que desenha um calendário mensal no desktop, **entre o
wallpaper e as janelas**, no estilo minimalista da imagem de referência
(título grande, ano pequeno à direita, linha fina, dias da semana e números).

Tudo é configurável: fontes, tamanhos e cores de cada elemento (título, ano,
dias da semana, números), idioma, nomes curtos/longos, primeiro dia da semana,
badge no dia atual, cor e transparência do fundo e posição na tela.

> **Versão suportada: Hyprland v0.57.x** (compilado e conferido contra o commit
> `6d6e935c3`, `v0.57.0-164`). A API de plugins muda entre versões, então para
> outro Hyprland pode ser preciso ajustar o `src/main.cpp` (os trechos
> dependentes da API estão marcados com `[API]`). A configuração é em **Lua**:
> o suporte ao `hyprland.conf` clássico foi removido do Hyprland nessa versão.

## Como instalar

Plugins do Hyprland são compilados contra **exatamente a versão** do Hyprland
que está rodando. Faça o build na própria máquina, com os headers instalados.

### 1. Dependências

- Compilador no mesmo nível do usado para o Hyprland 0.57: **GCC 14+** (ou
  Clang recente). `g++ --version` mostra o seu.
- `make`, `pkg-config`, `pango` e `cairo` (com headers).
- Headers do Hyprland: o pacote do Hyprland da distro costuma trazer
  (`hyprland` no Arch). Em outras, procure `hyprland-devel`; se compilou o
  Hyprland do código-fonte, rode `sudo make installheaders` nele.

Confira: `pkg-config --modversion hyprland` deve responder sem erro.

```bash
#   Arch:    sudo pacman -S base-devel pango cairo
#   Fedora:  sudo dnf install gcc-c++ make pkgconf pango-devel cairo-devel
#   Debian:  sudo apt install g++ make pkg-config libpango1.0-dev libcairo2-dev
```

### 2. Compilar e instalar

```bash
tar xzf hypr-calendar.tar.gz && cd hypr-calendar
make plugin            # gera build/hypr-calendar.so
make install           # copia para ~/.local/share/hypr-calendar/
```

Se o seu `g++` padrão for antigo: `make plugin CXX=g++-14`.

### 3. Carregar

Teste primeiro, sem mexer na config:

```bash
hyprctl plugin load ~/.local/share/hypr-calendar/hypr-calendar.so
hyprctl plugin list
hyprctl plugin unload ~/.local/share/hypr-calendar/hypr-calendar.so
```

Para carregar sempre, no seu `hyprland.lua` (modelo completo em
`examples/hyprland.lua`):

```lua
hl.plugin.load(os.getenv("HOME") .. "/.local/share/hypr-calendar/hypr-calendar.so")

hl.config({
    plugin = {
        calendar = {
            anchor   = "top_right",
            offset_x = 60,
            offset_y = 60,
        },
    },
})
```

Salvar o arquivo recarrega a config; as opções do calendário valem na hora.

**Alternativa com o hyprpm** (depois de publicar o repositório):
`hyprpm add https://github.com/SEU_USUARIO/hypr-calendar` e
`hyprpm enable hypr-calendar`. Se você ativou `ecosystem.enforce_permissions`,
libere o hyprpm com `hl.permission("/usr/(bin|local/bin)/hyprpm", "plugin", "allow")`.

### Atualizou o Hyprland? Recompile

O plugin se recusa a carregar se foi compilado para outra versão (você verá a
notificação "versão incompatível"). Depois de atualizar o Hyprland:
`make clean && make plugin && make install`.

## Testar sem instalar (sem Hyprland)

O desenho fica num núcleo separado (Cairo + Pango). O `calendar-preview` usa
exatamente o mesmo código do plugin e gera um PNG, então o que você vê aqui é
o que o plugin desenha. Só precisa de `g++`, `make` e `pango`/`cairo`.

```bash
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

# badge no dia atual, outro mês, tela HiDPI
build/calendar-preview -c examples/preview.conf --date 2026-12-25 --scale 2 \
    --set today_style=badge --set today_badge_color='#e63946' -o natal.png
```

As chaves do arquivo são as mesmas do plugin (`plugin:calendar:<chave>`).
Comentários começam com `# ` (um `#` seguido de espaço) ou ficam no fim da linha;
`#RRGGBB` continua sendo cor.

## Opções (`plugin:calendar:<nome>`)

Em Lua use tabelas aninhadas (`plugin = { calendar = { ... } }`) e **cores como
strings**: `"rgb(112233)"`, `"rgba(112233ee)"` ou `"0xAARRGGBB"`. No arquivo do
preview também vale `#RRGGBB`. Tamanhos são em px lógicos.

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

- O calendário é só visual: não captura cliques.
- A transparência afeta só o fundo; o texto e o badge mantêm a própria cor/alpha.
- Idiomas embutidos: en, pt, es, fr, de, it. Outros caem para inglês.
- Não reserva espaço de barras (waybar etc.): ajuste `offset_y`/`offset_x`.
- O calendário é desenhado logo acima do wallpaper, então fica **sob** as
  barras e as janelas (o que é o objetivo, mas uma janela maximizada o cobre).
