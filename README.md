# Futebol 2D

Futebol em vista superior, utilizando **C99 + OpenGL** e **FreeGLUT**. Um binário monolítico (`fut.c`) concentra simulação, regras, IA e renderização; o áudio é um módulo opcional (`audio.c` / `audio.h`) por trás da mesma API, com ou sem OpenAL em tempo de compilação.

Documentação de arquitetura e subsistemas: [**docs/DOCUMENTACAO.md**](docs/DOCUMENTACAO.md).

## Início rápido

```bash
gcc  -Wall -Wextra -O2 fut.c audio.c -o fut -lGL -lGLU -lglut -lopenal -lm
./fut
```

- O processo deve ser iniciado com **diretório de trabalho na raiz do repositório** (`audios/`, `video/` são caminhos relativos).
- **ffmpeg** deve estar no `PATH` se você quiser decodificação de MP3 no áudio e extração automática da intro a partir de `video/intro.mp4`.

Sem OpenAL (headers ausentes ou link sem `-lopenal`), o mesmo `audio.c` compila com implementações vazias; o jogo roda mudo.

```bash
gcc  -Wall -Wextra -O2 fut.c audio.c -o fut -lGL -lGLU -lglut -lm
```

## Dependências

| Camada | Pacotes típicos (Fedora) |
|--------|---------------------------|
| Toolchain | `gcc` |
| Gráfico | `mesa-libGL-devel`, `mesa-libGLU-devel`, `freeglut-devel` |
| Áudio (opcional) | `openal-soft-devel` |
| Runtime | `ffmpeg` (binário, não biblioteca de link) |

Equivalente Debian/Ubuntu: `build-essential`, `libgl1-mesa-dev`, `libglu1-mesa-dev`, `freeglut3-dev`, `libopenal-dev`, `ffmpeg`.

## Controles (resumo)

| Contexto | Time A | Time B (2P) |
|----------|--------|----------------|
| Movimento | Setas | W A S D |
| Sprint | C | R |
| Chute | Espaço | F |
| Menu cores | 1–0 (cor A), Q–P (cor B), F1/F2 modo, mouse, **Enter** inicia | idem |
| Intro | **Enter** → menu de cores | |
| Sair | **Esc** | |

## Layout do repositório

| Caminho | Conteúdo |
|---------|----------|
| `fut.c` | Jogo completo: estado, física, regras, IA, GLUT, intro |
| `audio.c`, `audio.h` | OpenAL + PCM via ffmpeg, ou no-op |
| `audios/` | MP3 referenciados em `audio.c` |
| `video/` | `intro.mp4` (opcional); `intro_frames/` gerado em runtime |

## Créditos

Algumas Partes foram  desenvolvidas com assistência de IA (ver  [**docs/DOCUMENTACAO.md**](docs/DOCUMENTACAO.md).).
