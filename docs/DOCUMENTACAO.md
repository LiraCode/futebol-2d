# Futebol 2D — arquitetura e guia técnico

Documento para quem vai **analisar, avaliar, manter, estender ou depurar** o projeto.

Este documento foi gerado a partir de um **Modelo**

O Projeto Foi testado apenas no **Linux** (**Fedora 43** ❤️ )

Os audios foram baixados **youtube** (NoCopyrightSounds) e **voicy.network** (inclusive trecho da música de love again - **Jhon Newman** gratuito com inclusão de créditos) e **sounddino.com** (efeitos sonoros gratuitos).

---

IA utilizada **Claude** da **Anthropic** (versão **Sonnet 4.6** - free) (bastante limitado em termos de requests por dia).

## 1. Escopo do sistema

| Entrada | Saída |
|---------|--------|
| Teclado, mouse (GLUT) | Imagem 2D ortográfica (~1366×860), ~60 atualizações/s via `glutTimerFunc(16, …)` |
| Arquivos em `audios/*.mp3` (opcional) | PCM → OpenAL |
| `video/intro.mp4` (opcional) + ffmpeg | Sequência PPM em `video/intro_frames/` → textura GL na intro |

---

## 2. Premissas de execução

1. **CWD = raiz do projeto** — todos os paths de asset são relativos (`audios/…`, `video/…`).
2. **ffmpeg** é invocado por `popen`/`system`; não há link com libav*. Se o binário não existir, áudio cai em síntese procedural ou silêncio; a intro cai em tela placeholder.
3. **Um thread** — GLUT e OpenAL no mesmo fluxo; não chamar GL de outro thread.
4. **Taxa lógica acoplada ao timer** — `update` avança o estado do jogo no mesmo ritmo que o redesenho; não há acumulador de delta-time explícito.

---

## 3. Mapa de módulos

```
┌─────────────────────────────────────────────────────────┐
│                        fut.c                             │
│  Estado global · regras · IA · física bola/jogadores     │
│  GLUT callbacks · OpenGL draw · intro (PPM → texture)    │
└───────────────┬─────────────────────────────────────────┘
                │ include + chamadas
                ▼
┌─────────────────────────────────────────────────────────┐
│              audio.h  ←→  audio.c                        │
│  API estável; implementação real ou stub (#ifdef)       │
└─────────────────────────────────────────────────────────┘
```

`fut.c` não exporta API: tudo é `static` exceto `main`. O único contrato estável entre unidades de compilação é **`audio.h`**.

---

## 4. Máquina de estados do produto

Enum `EstadoJogo` governa o que `display` desenha e o que `update` integra.

Fluxo nominal:

`TELA_INTRO` → `TELA_COR` → `JOGANDO` → `INTERVALO` (entre tempos) → `JOGANDO` → `GOL_MARCADO` / `FIM_JOGO` conforme eventos.

- **Inversão de lado:** `lados_invertidos` no segundo tempo; funções `ataca_para_direita`, `x_gol_*`, `progresso_ataque` encapsulam a geometria “para frente” de cada time.
- **Tempo de jogo** é contado em **frames** (`tempo_jogo`, `FRAMES_POR_TEMPO`, `FRAMES_PARTIDA`), não em segundos decimais — qualquer mudança de `glutTimerFunc` exige rever esses macros.

---

## 5. Modelo de dados (núcleo)

### `Jogador`

Posição/velocidade, alvo de movimento, time, número, cooldowns de passe/chute/tackle, animação de perna, **energia (fadiga)**, **raio de pressão**, flags de posse/marcação. A IA e o humano escrevem principalmente `alvo_*`, `vx/vy` e cooldowns; `aplicar_movimento` aplica limites de campo, sprint, goleiro e regras de bola parada.

### `Bola`

`x, y, vx, vy`, `posse` codificada como `time*100 + índice` ou `-1` (livre). Quem atualiza transições de posse, colisões e eventos de jogo parado é **`atualiza_bola`** em conjunto com o estado de `TipoReposicao` / timers de apito.

### Globais de sessão

Placar, modo 1P/2P, cores da paleta, jogador humano ativo por time, timers de intro/gol/intervalo/fim, estado de reposição. Tratar como **estado de aplicação único** — não há camadas de “cena” separadas em arquivos.

---

## 6. Subsistemas em `fut.c` (onde olhar)

| Subsistema | Responsabilidade | Funções âncora (não exaustivo) |
|------------|------------------|--------------------------------|
| **Regras / set pieces** | Laterais, escanteio, tiro de meta, distância mínima, kickoff com apito | `posicionar_times_reinicio`, `escolher_cobrador_reposicao`, `pode_pegar_bola`, `adversarios_longe_reposicao`, ramos em `atualiza_bola` |
| **Movimento** | Steering suave, separação, limites, fadiga | `mover_para_alvo`, `aplicar_movimento`, `separar_jogadores`, `atualizar_fadiga` |
| **Disputa**(sistema de fadiga e roubo de bola implementado usando IA) | Tackle com chance, ângulo, cooldown, fadiga | `tentar_tackle`, `tackle_valido_posicao`, `chance_tackle_fadiga` |
| **IA time completo** (Corrigido usando IA) | Pressão, passe, chute, zonas | `ia_jogador_full`, `zona_jogador`, `tentar_passe` |
| **IA companheiros 1P** (Corrigido usando IA) | Apoio ao humano sem bola | `ia_companheiro`, `atribuir_marcacoes` |
| **Humano** | Input, sprint, passe/chute, cobranças | `controle_humano`, `melhor_receptor_humano` |
| **Render** (Em torcida foi feito alguns ajustes (com IA) devido não ter ficado legal usando texturas ) | Campo, elenco, torcida procedural, HUD, overlays | `draw_campo`, `draw_jogador`, `draw_arquibancada_e_torcida`, `draw_hud` |
| **Intro** (implementado usando IA)| Descoberta de frames, carga PPM, textura | `intro_ensure_frames_extracted`, `intro_load_ppm_rgb`, `draw_tela_intro_video` |
| **Loop** | Integração frame | `update`, `display`, callbacks GLUT |

Para uma mudança de **regra de jogo**, o primeiro lugar a inspecionar é o bloco de **`atualiza_bola`** e os timers associados a `reposicao_*` / `saida_*`.

---

## 7. Contrato de áudio (`audio.h` implementado usando IA)

Todas as funções são **no-op seguras** quando OpenAL não foi compilado ou `audio_init` falhou.

| Função | Semântica |
|--------|------------|
| `audio_init` / `audio_shutdown` | Ciclo de vida do dispositivo e buffers |
| `audio_play_*` | One-shot SFX |
| `audio_play_intro` / `audio_stop_intro` | Loop da trilha de menu/intro |
| `audio_set_crowd_active` | Torcida ligada/desligada (ex.: intro) |
| `audio_tick_crowd` | Chamado no tick do jogo: volume da torcida e rotação de faixas |
| `audio_is_available` | Sucesso da inicialização OpenAL |

Implementação (`audio.c` com `AUDIO_OPENAL`, feito por IA):

- MP3 → PCM mono **22050 Hz s16le** via **ffmpeg** em `popen`.
- Fallback **procedural** se arquivo ausente (exceto conjunto de torcida: três MP3 ou loop sintético compartilhado).
- Torcida: fonte em loop com troca de buffer conforme ciclo/tempo.

---

## 8. Pipeline da intro

1. `intro_ensure_frames_extracted` verifica `video/intro_frames/frame_00000.ppm`.
2. Se ausente e existir `video/intro.mp4`, roda **ffmpeg** (fps e escala fixos no comando) gerando PPMs numerados.
3. `readdir` determina `intro_frame_count`.
4. A cada poucos frames de simulação, carrega o PPM correspondente em RAM, sobe para **textura 2D** (`intro_tex`), desenha quad full-screen.

Falha em qualquer etapa: `intro_extract_failed`, UI mínima com Enter para seguir.

---

## 9. Assets esperados

### Áudio (`audios/`)

Nomes exatos usados em `audio.c`:
alguns audios que estão na pasta ainda n foram implementados, abaixo estão os implementados:
- Efeitos: `chute.mp3`, `gol.mp3`, `escateio.mp3`, `lateral.mp3`, `startgol.mp3`
- Torcida: `tocidaAmbiance.mp3`, `torcida normal.mp3`, `bateria.mp3`
- Menu/intro: `intro.mp3`

### Vídeo

- `video/intro.mp4` — opcional; gera `video/intro_frames/frame_(numero do frame).ppm` sob demanda.

---

## 10. Constantes e tuning

Macros no topo de `fut.c` ajudam ajustar o jogo para uma jogabilidade mais equilibrada e evitar encher variaveis e funções desnecessárias.

---

## 11. Build recomendado para CI ou desenvolvimento


Linux
```bash
gcc  -Wall -Wextra -O2 -g fut.c audio.c -o fut -lGL -lGLU -lglut -lopenal -lm
```

Windows
```bash
gcc -Wall -Wextra -O2 -g fut.c audio.c -o fut -I"C:\Caminho\Para\Cabecalhos" -L"C:\Caminho\Para\Bibliotecas" -lGL -lGLU -lglut -lopenal -lm 
```
  

---

## 12. Extensões plausíveis (sem refator grande, Sugeridos pela IA)

- Substituir immediate mode por **VBO/VAO** mantendo a mesma geometria gerada.
- Extrair **máquina de estados** e `atualiza_bola` para `.c` separados quando o arquivo ultrapassar conforto de revisão.
- Trocar `popen(ffmpeg)` por **libmpg123** / **miniaudio** se distribuir binário sem depender do PATH.
