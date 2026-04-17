/*
 * FUTEBOL 2D - OpenGL/FreeGLUT
 *  PARTE DE IA DE JOGO GERADO POR IA (CLAUDE4.5)
 */

 

 #include <GL/glut.h>
 #include <math.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
 #include "audio.h"
 
 #ifndef M_PI
 #  define M_PI 3.14159265358979323846
 #endif
 
 /* ─────────────────────────────────────────
    CONSTANTES
 ───────────────────────────────────────── */
 #define LARGURA_JANELA   1366
 #define ALTURA_JANELA    860
 
 #define CAMPO_W   900.0f
 #define CAMPO_H   580.0f
 #define CAMPO_X   ((LARGURA_JANELA - CAMPO_W) * 0.5f)
 #define CAMPO_Y   ((ALTURA_JANELA - CAMPO_H) * 0.5f)
 
 #define RAIO_JOGADOR    10.0f
 #define RAIO_BOLA        7.0f
#define VELOCIDADE_MAX   1.18f
 #define VELOCIDADE_BOLA  3.5f
 #define ATRITO_BOLA      0.982f
 #define DIST_PASSE       110.0f
 #define DIST_CHUTE       140.0f
 #define DIST_TACKLE      18.0f
 #define DIST_MARCACAO    80.0f
 /* Distancia minima (em px) do adversario em bola parada */
 #define DIST_REPOSICAO_ADV  55.0f
 #define FRAMES_POR_TEMPO 10800
 #define FRAMES_PARTIDA   (FRAMES_POR_TEMPO * 2)
#define TEMPO_GOL_FRAMES 780   /* 13s @ 60fps */
 
 #define NUM_JOGADORES  11
 #define GOL_W   16.0f
 #define GOL_H   80.0f
 
 /* Área do pênalti */
 #define AREA_PENALTI_W  80.0f
 #define AREA_PENALTI_H  120.0f
 
 /* Fadiga somemte quando o jogador corre */
 #define ENERGIA_MAX          1.0f
 #define CUSTO_CORRIDA        0.000012f   /* por frame correndo */
 #define CUSTO_PARADO         0.000003f   /* recupera lentamente parado */
 #define LIMITE_FADIGA        0.35f       /* abaixo disso reduz vel/desarme */
 #define FATOR_VEL_FADIGA     0.70f       /* multiplicador de velocidade com fadiga */
 #define FATOR_TACKLE_FADIGA  0.55f       /* chance de desarme com fadiga */
 
 /* Raio de pressão */
 #define RAIO_PRESSAO_BASE    120.0f      /* raio normal para pressionar */
 #define RAIO_PRESSAO_DEFESA  160.0f      /* raio maior quando defende */
 
 /* Sprint do jogador humano */
#define VELOCIDADE_SPRINT    2.45f       /* vel máxima ao correr */
 #define CUSTO_SPRINT         0.00045f    /* gasta energia por frame de sprint */
 #define ENERGIA_MIN_SPRINT   0.15f       /* abaixo disso não pode correr */
#define FATOR_VEL_GOLEIRO    0.78f       /* goleiro mais lento */
 
typedef enum { TELA_COR, JOGANDO, INTERVALO, GOL_MARCADO, FIM_JOGO, TELA_INTRO } EstadoJogo;
 typedef enum { MODO_1P = 1, MODO_2P = 2 } ModoJogo;
 
 /* ─────────────────────────────────────────
    ESTRUTURAS
 ───────────────────────────────────────── */
 typedef struct {
     float x, y, vx, vy;
     float alvo_x, alvo_y;
     int   com_bola, time, numero;
     float base_x, base_y;
     float cd_passe, cd_chute, cd_tackle;
     float ang_perna;
     int   dir_perna;
     float vel_atual;
     int   marcando;
     float energia;          /* 0.0 a 1.0 — fadiga */
     float raio_pressao;     /* raio dinamico de pressao */
 } Jogador;
 
 typedef struct {
     float x, y, vx, vy;
     int   posse;
 } Bola;
 
 typedef struct {
     float r, g, b;
     char  nome[32];
 } CorTime;
 
 /* ─────────────────────────────────────────
    VARIÁVEIS GLOBAIS
 ───────────────────────────────────────── */
 static Jogador    jogadores[2][NUM_JOGADORES];
 static Bola       bola;
 static int        placar[2] = {0, 0};
static EstadoJogo estado    = TELA_INTRO;
 static ModoJogo   modo_jogo = MODO_1P;
 static int        cor_selecionada[2] = {0, 3};
 static int        tempo_jogo = 0;
 static int        tempo_gol  = 0;
 static int        tempo_intervalo = 0;
static int        tempo_fim_jogo = 0; /* contador para voltar ao menu após fim */
 static int        ultimo_gol = -1;
 static int        frame = 0;
 static int        etapa_atual = 1;   /* 1 = primeiro tempo, 2 = segundo tempo */
 static int        lados_invertidos = 0;

/* ─────────────────────────────────────────
   INTRO (video mp4 -> frames -> textura)
   ───────────────────────────────────────── */
static GLuint intro_tex = 0;
static int intro_ready = 0;
static int intro_frame_count = 0;
static int intro_last_idx = -1;
static int intro_extract_failed = 0;
 
 /* Jogador controlado pelo humano por time (-1 = IA) */
 static int jogador_humano[2] = {-1, -1};
 static int troca_humano_cd[2] = {0, 0}; /* cooldown da troca automática de marcador */
static float limite_vel_humano[2] = {0.f, 0.f}; /* limite atual do jogador controlado */
 
 /* Indicador visual de passe (linha de passe) */
 static int passe_visual_timer[2] = {0, 0};
 static float passe_visual_x[2]   = {0, 0};
 static float passe_visual_y[2]   = {0, 0};
 
 static CorTime paleta[] = {
     {0.9f, 0.1f, 0.1f, "Vermelho"},
     {0.1f, 0.3f, 0.9f, "Azul"},
     {0.1f, 0.7f, 0.1f, "Verde"},
     {1.0f, 0.8f, 0.0f, "Amarelo"},
     {0.6f, 0.1f, 0.8f, "Roxo"},
     {1.0f, 0.5f, 0.0f, "Laranja"},
     {0.0f, 0.7f, 0.8f, "Ciano"},
     {0.9f, 0.9f, 0.9f, "Branco"},
     {0.1f, 0.1f, 0.1f, "Preto"},
     {0.6f, 0.4f, 0.2f, "Marrom"},
 };
 #define NUM_CORES 10
 
 static int teclas[256];
 static int teclas_esp[256];
 
 /* Quem tocou por último na bola (time 0, 1 ou -1) — para lateral/escanteio */
 static int ultimo_toque = -1;
 
 /* Estado de lateral/escanteio */
 typedef enum { REPOSICAO_NENHUMA, LATERAL, ESCANTEIO, TIRO_META } TipoReposicao;
 static TipoReposicao reposicao_tipo   = REPOSICAO_NENHUMA;
 static int           reposicao_time   = -1;   /* time que bota a bola */
 static float         reposicao_bola_x = 0;
 static float         reposicao_bola_y = 0;
 static int           reposicao_timer  = 0;    /* frames de espera antes de jogar */
 static int           reposicao_jogador = -1;  /* jogador escolhido para cobrar */
 static int           reposicao_timer_humano = 0; /* limite para cobrança humana (escanteio) */
static int           reposicao_apito_timer = 0; /* espera ~3s e libera no apito */
 
 /* Saída de bola (meio): proteção do 1o toque; adversário não desarma até a jogada começar */
 static int saida_centro_timer = 0;
 static int posse_inicial_saida = -1;
 static int time_saida_kickoff = 0;
static int saida_apito_timer = 0; /* espera ~3s e libera no apito longo */
 static int gol_crowd_boost = 0;
 static int audio_danger_cd = 0;
 
 static float formacao[2][NUM_JOGADORES][2] = {
     /* TIME 0 — ataca para a direita */
     {
         {0.06f, 0.50f},
         {0.22f, 0.20f},{0.22f, 0.40f},
         {0.22f, 0.60f},{0.22f, 0.80f},
         {0.42f, 0.18f},{0.42f, 0.38f},
         {0.42f, 0.62f},{0.42f, 0.82f},
         {0.62f, 0.35f},{0.62f, 0.65f},
     },
     /* TIME 1 — ataca para a esquerda */
     {
         {0.94f, 0.50f},
         {0.78f, 0.20f},{0.78f, 0.40f},
         {0.78f, 0.60f},{0.78f, 0.80f},
         {0.58f, 0.18f},{0.58f, 0.38f},
         {0.58f, 0.62f},{0.58f, 0.82f},
         {0.38f, 0.35f},{0.38f, 0.65f},
     }
 };
 
 /* ─────────────────────────────────────────
    FUNÇÕES AUXILIARES
 ───────────────────────────────────────── */
 static float fclamp(float v, float mn, float mx);
 static int ataca_para_direita(int t);
 
 static float fdist(float ax, float ay, float bx, float by) {
     float dx = ax-bx, dy = ay-by;
     return sqrtf(dx*dx+dy*dy);
 }
 
 static int escolher_cobrador_reposicao(int t) {
     int j;
     int melhor = 1;
     float dmin = 999999.f;
 
     if (t < 0 || t > 1) return -1;
 
     for (j = 1; j < NUM_JOGADORES; j++) { /* evita goleiro por padrao */
         float d = fdist(jogadores[t][j].x, jogadores[t][j].y,
                         reposicao_bola_x, reposicao_bola_y);
         if (d < dmin) { dmin = d; melhor = j; }
     }
 
     return melhor;
 }
 static void alvo_cobrador_reposicao(float *ax, float *ay) {
     *ax = reposicao_bola_x;
     *ay = reposicao_bola_y;
 
     if (reposicao_tipo == TIRO_META && reposicao_time >= 0 && reposicao_time <= 1) {
         float offset = (reposicao_time == 0) ? -18.f : 18.f;
         *ax = reposicao_bola_x + offset;
         *ay = reposicao_bola_y;
         if (*ax < CAMPO_X + RAIO_JOGADOR) *ax = CAMPO_X + RAIO_JOGADOR;
         if (*ax > CAMPO_X + CAMPO_W - RAIO_JOGADOR) *ax = CAMPO_X + CAMPO_W - RAIO_JOGADOR;
         if (*ay < CAMPO_Y + RAIO_JOGADOR) *ay = CAMPO_Y + RAIO_JOGADOR;
         if (*ay > CAMPO_Y + CAMPO_H - RAIO_JOGADOR) *ay = CAMPO_Y + CAMPO_H - RAIO_JOGADOR;
     }
 }
 static int pode_pegar_bola(int t, int idx) {
     if (reposicao_tipo == REPOSICAO_NENHUMA) return 1;
     return (t == reposicao_time && idx == reposicao_jogador);
 }
 
 /* Na saída de bola: adversários não podem desarmar o cobrador até a bola ser jogada */
 static int pode_desarmar_na_saida(int tackler_t) {
     if (saida_centro_timer <= 0 || bola.posse < 0) return 1;
     {
         int c = bola.posse / 100;
         if (tackler_t != c && c == time_saida_kickoff) return 0;
     }
     return 1;
 }
 
 static void ajustar_posicoes_saida_centro(int time_saida) {
     float mid = CAMPO_X + CAMPO_W * 0.5f;
     float cy = CAMPO_Y + CAMPO_H * 0.5f;
     float marg = 52.f;
     int idx_k = jogador_humano[time_saida];
     int t, j;
 
     if (idx_k < 0 || idx_k >= NUM_JOGADORES) idx_k = 9;
     for (t = 0; t < 2; t++) {
         int own_left = ataca_para_direita(t) ? 1 : 0;
         float xmin = CAMPO_X + RAIO_JOGADOR + 6.f;
         float xmax = CAMPO_X + CAMPO_W - RAIO_JOGADOR - 6.f;
         for (j = 0; j < NUM_JOGADORES; j++) {
             Jogador *p = &jogadores[t][j];
             if (j == 0) {
                 float gx = ataca_para_direita(t) ? (CAMPO_X + RAIO_JOGADOR * 2.5f)
                                                  : (CAMPO_X + CAMPO_W - RAIO_JOGADOR * 2.5f);
                 p->x = gx;
                 p->y = cy + (((j + t) % 3) - 1) * 18.f;
                 p->y = fclamp(p->y, CAMPO_Y + GOL_H * 0.35f, CAMPO_Y + CAMPO_H - GOL_H * 0.35f);
                 p->base_x = p->x; p->base_y = p->y;
                 p->alvo_x = p->x; p->alvo_y = p->y;
                 p->vx = p->vy = 0;
                 continue;
             }
             if (t == time_saida && j == idx_k) {
                 p->x = mid; p->y = cy;
                 p->base_x = mid; p->base_y = cy;
                 p->alvo_x = mid; p->alvo_y = cy;
                 p->vx = p->vy = 0;
                 continue;
             }
             if (own_left) {
                 float maxx = mid - marg;
                 if (p->x > maxx)
                     p->x = maxx - fminf(38.f, (p->x - maxx) * 0.62f);
                 if (p->x < xmin) p->x = xmin;
             } else {
                 float minx = mid + marg;
                 if (p->x < minx)
                     p->x = minx + fminf(38.f, (minx - p->x) * 0.62f);
                 if (p->x > xmax) p->x = xmax;
             }
             p->y = fclamp(p->y, CAMPO_Y + 20.f, CAMPO_Y + CAMPO_H - 20.f);
             p->base_x = p->x; p->base_y = p->y;
             p->alvo_x = p->x; p->alvo_y = p->y;
             p->vx = p->vy = 0;
         }
     }
 }
 static int adversarios_longe_reposicao(void) {
     int adv, j;
     float dmin = RAIO_JOGADOR + RAIO_BOLA + DIST_REPOSICAO_ADV;
 
     if (reposicao_tipo == REPOSICAO_NENHUMA || reposicao_time < 0 || reposicao_time > 1) return 1;
 
     adv = 1 - reposicao_time;
     for (j = 0; j < NUM_JOGADORES; j++) {
         float d = fdist(jogadores[adv][j].x, jogadores[adv][j].y,
                         reposicao_bola_x, reposicao_bola_y);
         if (d < dmin) return 0;
     }
     return 1;
 }
 static int ataca_para_direita(int t) {
     return ((t == 0) && !lados_invertidos) || ((t == 1) && lados_invertidos);
 }
 static float x_gol_adversario(int t) {
     return ataca_para_direita(t) ? (CAMPO_X + CAMPO_W + GOL_W) : (CAMPO_X - GOL_W);
 }
 static float x_gol_proprio(int t) {
     return ataca_para_direita(t) ? CAMPO_X : (CAMPO_X + CAMPO_W);
 }
 static float progresso_ataque(int t, float x) {
     return ataca_para_direita(t) ? x : (CAMPO_X + CAMPO_W - x);
 }
 static void posicionar_times_reinicio(int time_saida, int reset_partida, int apito_longo) {
     int t, j;
     for (t = 0; t < 2; t++) {
         for (j = 0; j < NUM_JOGADORES; j++) {
             float fx = formacao[t][j][0];
             Jogador *p = &jogadores[t][j];
             if (lados_invertidos) fx = 1.0f - fx;
             p->base_x = CAMPO_X + fx * CAMPO_W;
             p->base_y = CAMPO_Y + formacao[t][j][1] * CAMPO_H;
             p->x = p->base_x; p->y = p->base_y;
             p->vx = p->vy = 0;
             p->alvo_x = p->base_x; p->alvo_y = p->base_y;
             p->com_bola = 0;
             p->time = t;
             p->numero = j + 1;
             p->cd_passe = p->cd_chute = p->cd_tackle = 0;
             p->ang_perna = 0;
             p->dir_perna = 1;
             p->vel_atual = 0;
             p->marcando = -1;
             if (reset_partida) p->energia = ENERGIA_MAX;
             else {
                 p->energia += 0.35f; /* intervalo recupera energia */
                 if (p->energia > ENERGIA_MAX) p->energia = ENERGIA_MAX;
             }
             p->raio_pressao = RAIO_PRESSAO_BASE;
         }
         passe_visual_timer[t] = 0;
     }
 
     ajustar_posicoes_saida_centro(time_saida);
 
     bola.x = CAMPO_X + CAMPO_W * 0.5f;
     bola.y = CAMPO_Y + CAMPO_H * 0.5f;
     bola.vx = bola.vy = 0;
     reposicao_tipo = REPOSICAO_NENHUMA;
     reposicao_time = -1;
     reposicao_timer = 0;
     reposicao_jogador = -1;
     reposicao_timer_humano = 0;
    reposicao_apito_timer = 0;
     ultimo_toque = -1;
     frame = 0;
 
     {
         int idx_saida = jogador_humano[time_saida];
         if (idx_saida < 0 || idx_saida >= NUM_JOGADORES) idx_saida = 9;
         bola.posse = time_saida * 100 + idx_saida;
         jogadores[time_saida][idx_saida].com_bola = 1;
         posse_inicial_saida = bola.posse;
         time_saida_kickoff = time_saida;
         saida_centro_timer = 120;
     }
    saida_apito_timer = apito_longo ? 180 : 0;
    if (apito_longo && saida_apito_timer <= 0) audio_play_apito_longo();
 }
 static float fclamp(float v, float mn, float mx) {
     return v < mn ? mn : v > mx ? mx : v;
 }
 static void normalize(float *vx, float *vy) {
     float m = sqrtf(*vx**vx + *vy**vy);
     if (m > 0.001f) { *vx /= m; *vy /= m; }
 }
 static void draw_text(float x, float y, const char *s, void *font) {
     glRasterPos2f(x, y);
     while (*s) glutBitmapCharacter(font, *s++);
 }

static int bitmap_text_width(void *font, const char *s) {
    int w = 0;
    while (*s) {
        w += glutBitmapWidth(font, *s);
        s++;
    }
    return w;
}

static void draw_text_centered(float cx, float y, const char *s, void *font) {
    float x = cx - (float)bitmap_text_width(font, s) * 0.5f;
    draw_text(x, y, s, font);
}
 
 static int tackle_valido_posicao(Jogador *ladrao, Jogador *portador, float alcance) {
     float d = fdist(ladrao->x, ladrao->y, portador->x, portador->y);
     if (d > alcance) return 0;
 
     /* Evita desarme "telepático" pelas costas quando ainda está longe */
     {
         float mvx = portador->vx, mvy = portador->vy;
         float m = sqrtf(mvx*mvx + mvy*mvy);
         if (m > 0.25f) {
             float tx = ladrao->x - portador->x;
             float ty = ladrao->y - portador->y;
             normalize(&mvx, &mvy);
             normalize(&tx, &ty);
             {
                 float frente = mvx*tx + mvy*ty; /* >0 à frente do portador */
                 if (frente < -0.10f && d > alcance * 0.65f) return 0;
             }
         }
     }
     return 1;
 }
 
 static void atualizar_troca_humano_sem_bola(int t) {
     int j;
     int idx_atual, melhor = -1;
     float d_atual = 999999.f, d_melhor = 999999.f;
     float alvo_x = bola.x, alvo_y = bola.y;
     int adv = 1 - t;
 
     if (t < 0 || t > 1) return;
     if (reposicao_tipo != REPOSICAO_NENHUMA && reposicao_time == t) return;
 
     /* Se meu time tem a bola, não força troca automática */
     if (bola.posse != -1 && bola.posse / 100 == t) return;
 
     /* Quando adversário tem a bola, prioriza portador */
     if (bola.posse != -1 && bola.posse / 100 == adv) {
         int port = bola.posse % 100;
         if (port >= 0 && port < NUM_JOGADORES) {
             alvo_x = jogadores[adv][port].x;
             alvo_y = jogadores[adv][port].y;
         }
     }
 
     idx_atual = jogador_humano[t];
     if (idx_atual >= 0 && idx_atual < NUM_JOGADORES) {
         d_atual = fdist(jogadores[t][idx_atual].x, jogadores[t][idx_atual].y, alvo_x, alvo_y);
     }
 
     for (j = 0; j < NUM_JOGADORES; j++) {
         float d = fdist(jogadores[t][j].x, jogadores[t][j].y, alvo_x, alvo_y);
         if (j == 0) d += 8.f; /* evita trocar para goleiro à toa */
         if (d < d_melhor) { d_melhor = d; melhor = j; }
     }
 
     if (melhor < 0) return;
 
     if (troca_humano_cd[t] > 0) {
         troca_humano_cd[t]--;
         /* Durante cooldown, só troca se o atual estiver bem pior */
         if (idx_atual >= 0 && idx_atual < NUM_JOGADORES && (d_atual - d_melhor) < 26.f) return;
     }
 
     if (idx_atual != melhor) {
         /* Troca mais agressiva para reação defensiva rápida */
         if (idx_atual < 0 || idx_atual >= NUM_JOGADORES || (d_atual - d_melhor) > 8.f || d_atual > 42.f) {
             jogador_humano[t] = melhor;
             troca_humano_cd[t] = 10;
         }
     }
 }
 
 /* ─────────────────────────────────────────
    INICIALIZAÇÃO
 ───────────────────────────────────────── */
 static void init_jogo(void) {
     int apos_gol = (estado == GOL_MARCADO);
     int ts;
     srand((unsigned)time(NULL));
     /* Preserva o tempo ao reiniciar após gol; zera apenas no início de partida nova */
     if (!apos_gol) {
         tempo_jogo = 0;
         etapa_atual = 1;
         lados_invertidos = 0;
     }
 
     jogador_humano[0] = 9;
     jogador_humano[1] = (modo_jogo == MODO_2P) ? 9 : -1;
     /* Após gol: quem sofreu o gol dá a saída de bola (regra usual) */
     ts = apos_gol ? (1 - ultimo_gol) : 0;
     posicionar_times_reinicio(ts, apos_gol ? 0 : 1, 1);
 }
 
 /* ─────────────────────────────────────────
    MOVIMENTAÇÃO COMPARTILHADA
 ───────────────────────────────────────── */
 static void mover_para_alvo(Jogador *p, float ax, float ay, float fator) {
     float dx = ax - p->x, dy = ay - p->y;
     float d  = sqrtf(dx*dx+dy*dy);
     float vel = VELOCIDADE_MAX * fator;
     if (d > 1.5f) {
         float f = (d > 30.f ? 1.f : d/30.f);
         p->vx = (dx/d)*vel*f;
         p->vy = (dy/d)*vel*f;
     } else {
         p->vx *= 0.6f; p->vy *= 0.6f;
     }
 }
 
 static void separar_jogadores(Jogador *p, int t, int idx) {
     int tt, jj;
     for (tt = 0; tt < 2; tt++) {
         for (jj = 0; jj < NUM_JOGADORES; jj++) {
             if (tt == t && jj == idx) continue;
             Jogador *o = &jogadores[tt][jj];
             float sdx = p->x - o->x, sdy = p->y - o->y;
             float sd = sqrtf(sdx*sdx+sdy*sdy);
             if (sd < RAIO_JOGADOR*2.0f && sd > 0.1f) {
                 /* Empurra posição diretamente — não altera velocidade,
                    evita acúmulo de impulso entre frames */
                 float overlap = (RAIO_JOGADOR*2.0f - sd) * 0.4f;
                 p->x += (sdx/sd)*overlap;
                 p->y += (sdy/sd)*overlap;
             }
         }
     }
 }
 
 static void aplicar_movimento(Jogador *p, int idx, int t) {
    
     float spd = sqrtf(p->vx*p->vx + p->vy*p->vy);
    float vmax = VELOCIDADE_MAX;
    int time_humano = (t == 0 || modo_jogo == MODO_2P);
    if (idx == 0) vmax *= FATOR_VEL_GOLEIRO;
    if (time_humano && idx == jogador_humano[t] && limite_vel_humano[t] > 0.01f) {
        vmax = limite_vel_humano[t];
    }
     if (spd > vmax) { p->vx = (p->vx/spd)*vmax; p->vy = (p->vy/spd)*vmax; }
     p->vel_atual = sqrtf(p->vx*p->vx + p->vy*p->vy);
     p->x += p->vx; p->y += p->vy;
    
     separar_jogadores(p, t, idx);
     if (idx > 0) {
         p->x = fclamp(p->x, CAMPO_X+RAIO_JOGADOR, CAMPO_X+CAMPO_W-RAIO_JOGADOR);
         p->y = fclamp(p->y, CAMPO_Y+RAIO_JOGADOR, CAMPO_Y+CAMPO_H-RAIO_JOGADOR);
     } else {
         float gx = ataca_para_direita(t) ? CAMPO_X+RAIO_JOGADOR*2 : CAMPO_X+CAMPO_W-RAIO_JOGADOR*2;
         p->x = fclamp(p->x, gx-45, gx+45);
         p->y = fclamp(p->y, CAMPO_Y+CAMPO_H*0.5f-GOL_H*0.9f,
                             CAMPO_Y+CAMPO_H*0.5f+GOL_H*0.9f);
     }
 
     /* Regra de jogo: adversarios devem manter distancia na cobranca */
     if (reposicao_tipo != REPOSICAO_NENHUMA && t != reposicao_time) {
         float dx = p->x - reposicao_bola_x;
         float dy = p->y - reposicao_bola_y;
         float d  = sqrtf(dx*dx + dy*dy);
         float dmin = RAIO_JOGADOR + RAIO_BOLA + DIST_REPOSICAO_ADV;
         if (d < dmin) {
             if (d > 0.001f) {
                 p->x = reposicao_bola_x + (dx/d) * dmin;
                 p->y = reposicao_bola_y + (dy/d) * dmin;
             } else {
                 /* Caso raro em cima da bola: empurra para fora */
                 p->x = reposicao_bola_x + dmin;
             }
 
             if (idx > 0) {
                 p->x = fclamp(p->x, CAMPO_X+RAIO_JOGADOR, CAMPO_X+CAMPO_W-RAIO_JOGADOR);
                 p->y = fclamp(p->y, CAMPO_Y+RAIO_JOGADOR, CAMPO_Y+CAMPO_H-RAIO_JOGADOR);
             } else {
                 float gx = ataca_para_direita(t) ? CAMPO_X+RAIO_JOGADOR*2 : CAMPO_X+CAMPO_W-RAIO_JOGADOR*2;
                 p->x = fclamp(p->x, gx-45, gx+45);
                 p->y = fclamp(p->y, CAMPO_Y+CAMPO_H*0.5f-GOL_H*0.9f,
                                     CAMPO_Y+CAMPO_H*0.5f+GOL_H*0.9f);
             }
         }
     }
 
     /* Saída de bola: adversários mantêm distância mínima do centro (círculo) */
     if (saida_centro_timer > 0 && t != time_saida_kickoff && idx > 0) {
         float rcx = CAMPO_X + CAMPO_W * 0.5f, rcy = CAMPO_Y + CAMPO_H * 0.5f;
         float dx = p->x - rcx, dy = p->y - rcy;
         float dist = sqrtf(dx * dx + dy * dy);
         float minr = 86.f;
         if (dist < minr && dist > 0.01f) {
             p->x = rcx + (dx / dist) * minr;
             p->y = rcy + (dy / dist) * minr;
             if (idx > 0) {
                 p->x = fclamp(p->x, CAMPO_X + RAIO_JOGADOR, CAMPO_X + CAMPO_W - RAIO_JOGADOR);
                 p->y = fclamp(p->y, CAMPO_Y + RAIO_JOGADOR, CAMPO_Y + CAMPO_H - RAIO_JOGADOR);
             }
         }
     }
 
     if (p->vel_atual > 0.4f) {
         p->ang_perna += p->dir_perna * p->vel_atual * 2.5f;
         if (fabsf(p->ang_perna) > 28.f) p->dir_perna *= -1;
     } else {
         p->ang_perna *= 0.8f;
     }
 }
 
 /* ─────────────────────────────────────────
      AUXILIAR DE ROUBADA DE BOLA
 ───────────────────────────────────────── */
 static int tentar_tackle(Jogador *p, int t, int idx, int adv_idx_time, int adv_idx_jogador) {
     Jogador *alvo = &jogadores[adv_idx_time][adv_idx_jogador];
     if (!alvo->com_bola) return 0;
     if (!pode_desarmar_na_saida(t)) return 0;
     float alcance = DIST_TACKLE * 1.35f;
     if (p->cd_tackle <= 0 && tackle_valido_posicao(p, alvo, alcance)) {
         float chance = ((float)rand()/RAND_MAX);
         alvo->com_bola = 0;
         if (chance < 0.38f) {
             /* Roubo limpo com posse */
             bola.posse = t * 100 + idx;
             p->com_bola = 1;
             bola.vx = 0; bola.vy = 0;
         } else if (chance < 0.72f) {
             /* Desarme parcial: bola espirra */
             bola.posse = -1;
             {
                 float dvx = p->x - alvo->x, dvy = p->y - alvo->y;
                 float ruido = (((float)rand()/RAND_MAX)-0.5f)*1.2f;
                 normalize(&dvx, &dvy);
                 bola.vx = dvx*1.9f + ruido;
                 bola.vy = dvy*1.9f + ruido;
             }
         } else {
             /* Erra o bote: mantém a posse */
             alvo->com_bola = 1;
             p->cd_tackle = 0.45f;
             return 0;
         }
         p->cd_tackle = 1.0f;
         return 1;
     }
     return 0;
 }
 
 /* ─────────────────────────────────────────
    ZONAS DE ATUAÇÃO POR FUNÇÃO
 ───────────────────────────────────────── */
 static void zona_jogador(int t, int idx, float *x_def, float *x_atk) {
     float meio  = CAMPO_X + CAMPO_W * 0.5f;
     float meu_gol = x_gol_proprio(t);
     float gol_adv = ataca_para_direita(t) ? (CAMPO_X + CAMPO_W) : CAMPO_X;
 
     if (idx == 0) {
         float area = CAMPO_W * 0.10f;
         *x_def = ataca_para_direita(t) ? meu_gol : (meu_gol - area);
         *x_atk = ataca_para_direita(t) ? (meu_gol + area) : meu_gol;
     } else if (idx <= 4) {
         float limite_rec = CAMPO_W * 0.08f;
         float limite_av  = CAMPO_W * 0.10f;
         *x_def = ataca_para_direita(t) ? (meu_gol + limite_rec) : (meu_gol - limite_rec);
         *x_atk = ataca_para_direita(t) ? (meio + limite_av) : (meio - limite_av);
     } else if (idx <= 8) {
         float limite_rec = CAMPO_W * 0.15f;
         *x_def = ataca_para_direita(t) ? (meu_gol + limite_rec) : (meu_gol - limite_rec);
         *x_atk = ataca_para_direita(t) ? (gol_adv - CAMPO_W*0.05f) : (gol_adv + CAMPO_W*0.05f);
     } else {
         float limite_rec = CAMPO_W * 0.10f;
         *x_def = ataca_para_direita(t) ? (meio - limite_rec) : (meio + limite_rec);
         *x_atk = ataca_para_direita(t) ? (gol_adv - CAMPO_W*0.02f) : (gol_adv + CAMPO_W*0.02f);
     }
     if (ataca_para_direita(t) && *x_def > *x_atk) *x_def = *x_atk;
     if (!ataca_para_direita(t) && *x_def < *x_atk) *x_def = *x_atk;
 }
 
 static void pos_defensiva(int t, int idx, float fator_pressao) {
     Jogador *p = &jogadores[t][idx];
     float meu_gol_x = x_gol_proprio(t);
     float meu_gol_y = CAMPO_Y + CAMPO_H * 0.5f;
     float x_def, x_atk;
     zona_jogador(t, idx, &x_def, &x_atk);
 
     float px = meu_gol_x + (bola.x - meu_gol_x) * fator_pressao;
     float py = meu_gol_y + (bola.y - meu_gol_y) * fator_pressao;
 
     float offset_y = ((float)((idx % 4) + 1) - 2.5f) * 35.f;
     py += offset_y;
 
     float mix = 0.55f;
     px = px*(1.f-mix) + p->base_x*mix;
     py = py*(1.f-mix) + p->base_y*mix;
 
     if (ataca_para_direita(t)) px = fclamp(px, x_def, x_atk);
     else                       px = fclamp(px, x_atk, x_def);
     py = fclamp(py, CAMPO_Y + 8.f, CAMPO_Y + CAMPO_H - 8.f);
 
     p->alvo_x = px;
     p->alvo_y = py;
 }
 
 /* ─────────────────────────────────────────
    PASSE DA IA
 ───────────────────────────────────────── */
 static int tentar_passe(int t, int idx, int alvo_preferencial) {
     Jogador *p = &jogadores[t][idx];
     if (p->cd_passe > 0) return 0;
     int adv = 1 - t;
     int melhor = -1;
     float score_max = -999999.f;
     int j;
 
     for (j = 0; j < NUM_JOGADORES; j++) {
         if (j == idx) continue;
         Jogador *comp = &jogadores[t][j];
         float dc = fdist(p->x, p->y, comp->x, comp->y);
         if (dc > DIST_PASSE || dc < 20.f) continue;
 
         float av = progresso_ataque(t, comp->x);
         float pressao_comp = 999.f;
         { int k; for (k = 0; k < NUM_JOGADORES; k++) {
             float dd = fdist(comp->x, comp->y, jogadores[adv][k].x, jogadores[adv][k].y);
             if (dd < pressao_comp) pressao_comp = dd;
         }}
         float s = av - dc*0.2f + pressao_comp*0.3f;
         if (j == alvo_preferencial) s += 80.f;
         if (s > score_max) { score_max = s; melhor = j; }
     }
 
     if (melhor >= 0 && score_max > 0.f) {
         Jogador *dest = &jogadores[t][melhor];
         float dvx = dest->x - p->x, dvy = dest->y - p->y;
         normalize(&dvx, &dvy);
         bola.vx = dvx * VELOCIDADE_BOLA * 0.9f;
         bola.vy = dvy * VELOCIDADE_BOLA * 0.9f;
         bola.posse = -1;
         p->com_bola = 0;
         p->cd_passe = 0.9f;
         ultimo_toque = t;
         return 1;
     }
     return 0;
 }
 
 /* ─────────────────────────────────────────
    SISTEMA DE PASSE PARA HUMANO
 ───────────────────────────────────────── */
 static int melhor_receptor_humano(int t, int idx) {
     Jogador *p = &jogadores[t][idx];
     int adv = 1 - t;
     int melhor = -1;
     float score_max = -999999.f;
     int j;
 
     /* Direção preferida: movimento do jogador ou, se parado, rumo ao gol */
     float spd = sqrtf(p->vx*p->vx + p->vy*p->vy);
     float dir_x, dir_y;
     if (spd > 0.3f) {
         dir_x = p->vx / spd;
         dir_y = p->vy / spd;
     } else {
         float cx_gol = ataca_para_direita(t) ? (CAMPO_X+CAMPO_W) : CAMPO_X;
         dir_x = cx_gol - p->x; dir_y = 0;
         normalize(&dir_x, &dir_y);
     }
 
     for (j = 0; j < NUM_JOGADORES; j++) {
         if (j == idx) continue;
         Jogador *comp = &jogadores[t][j];
         float dc = fdist(p->x, p->y, comp->x, comp->y);
         /* Alcance do passe humano um pouco maior que IA */
         if (dc > DIST_PASSE * 1.4f || dc < 15.f) continue;
 
         /* Vetor até o companheiro */
         float vx2 = comp->x - p->x, vy2 = comp->y - p->y;
         normalize(&vx2, &vy2);
 
         /* Alinhamento com a direção desejada */
         float alinhamento = dir_x*vx2 + dir_y*vy2; /* -1 a 1 */
 
         /* Avanço do companheiro */
         float av = progresso_ataque(t, comp->x);
 
         /* Pressão adversária sobre o companheiro */
         float pressao_comp = 999.f;
         { int k; for (k = 0; k < NUM_JOGADORES; k++) {
             float dd = fdist(comp->x, comp->y, jogadores[adv][k].x, jogadores[adv][k].y);
             if (dd < pressao_comp) pressao_comp = dd;
         }}
 
         float s = av*0.5f + alinhamento*60.f + pressao_comp*0.25f - dc*0.15f;
         if (s > score_max) { score_max = s; melhor = j; }
     }
 
     return (score_max > -500.f) ? melhor : -1;
 }
 
 static void grudar_bola(Jogador *p, int t) {
     float spd = sqrtf(p->vx*p->vx + p->vy*p->vy);
     float ang = atan2f(p->vy, p->vx);
     if (spd < 0.1f) ang = ataca_para_direita(t) ? 0.f : (float)M_PI;
     bola.x = p->x + cosf(ang)*(RAIO_JOGADOR + RAIO_BOLA + 1);
     bola.y = p->y + sinf(ang)*(RAIO_JOGADOR + RAIO_BOLA + 1);
 }
 
 /* ─────────────────────────────────────────
    SISTEMA DE FADIGA
 ───────────────────────────────────────── */
 static void atualizar_fadiga(Jogador *p) {
     float spd = sqrtf(p->vx*p->vx + p->vy*p->vy);
     if (spd > 0.4f) {
         /* Correndo: gasta energia proporcional à velocidade */
         p->energia -= CUSTO_CORRIDA * (spd / VELOCIDADE_MAX);
     } else {
         /* Parado/lento: recupera energia */
         p->energia += CUSTO_PARADO;
     }
     if (p->energia < 0.f)       p->energia = 0.f;
     if (p->energia > ENERGIA_MAX) p->energia = ENERGIA_MAX;
 
     /* Raio de pressão reduz com fadiga */
     float fator = (p->energia < LIMITE_FADIGA)
                   ? (p->energia / LIMITE_FADIGA) : 1.f;
     p->raio_pressao = RAIO_PRESSAO_BASE * (0.6f + 0.4f * fator);
 }
 
 /* Retorna fator de velocidade considerando fadiga */
 static float fator_vel_fadiga(Jogador *p) {
     if (p->energia < LIMITE_FADIGA)
         return FATOR_VEL_FADIGA + (1.f - FATOR_VEL_FADIGA)
                * (p->energia / LIMITE_FADIGA);
     return 1.f;
 }
 
 /* Retorna chance de desarme considerando fadiga (0..1) */
 static float chance_tackle_fadiga(Jogador *p) {
     if (p->energia < LIMITE_FADIGA)
         return FATOR_TACKLE_FADIGA + (1.f - FATOR_TACKLE_FADIGA)
                * (p->energia / LIMITE_FADIGA);
     return 1.f;
 }
 
 /* ─────────────────────────────────────────
    IA DE JOGO (GERADO POR IA)
 ───────────────────────────────────────── */
 static void ia_jogador_full(int t, int idx) {
     Jogador *p = &jogadores[t][idx];
     int adv = 1 - t;
     int j;
 
     if (reposicao_tipo != REPOSICAO_NENHUMA && reposicao_time == t) {
         float alvo_x = reposicao_bola_x, alvo_y = reposicao_bola_y;
         alvo_cobrador_reposicao(&alvo_x, &alvo_y);
         if (idx == reposicao_jogador) {
             mover_para_alvo(p, alvo_x, alvo_y, 1.0f);
         } else {
             p->alvo_x = p->base_x;
             p->alvo_y = p->base_y;
             mover_para_alvo(p, p->alvo_x, p->alvo_y, 1.0f);
         }
         aplicar_movimento(p, idx, t);
         return;
     }
 
     /* Cooldowns */
     if (p->cd_passe  > 0) p->cd_passe  -= 0.016f;
     if (p->cd_chute  > 0) p->cd_chute  -= 0.016f;
     if (p->cd_tackle > 0) p->cd_tackle -= 0.016f;
 
     /* Fadiga */
     atualizar_fadiga(p);
     float fv = fator_vel_fadiga(p);
 
     float cx_gol = x_gol_adversario(t);
     float cy_gol = CAMPO_Y + CAMPO_H*0.5f;
     float x_def, x_atk;
     zona_jogador(t, idx, &x_def, &x_atk);
 
     /* Situação geral */
     int meu_time_tem = 0, quem_meu = -1;
     int adv_tem      = 0, adv_quem = -1;
     for (j = 0; j < NUM_JOGADORES; j++) {
         if (jogadores[t][j].com_bola)   { meu_time_tem = 1; quem_meu = j; }
         if (jogadores[adv][j].com_bola) { adv_tem = 1;      adv_quem = j; }
     }
 
     /* ── COM A BOLA ── */
     if (p->com_bola) {
         float d_gol = fdist(p->x, p->y, cx_gol, cy_gol);
 
         float area_x_esq = ataca_para_direita(t) ? (CAMPO_X + CAMPO_W - AREA_PENALTI_W) : CAMPO_X;
         float area_x_dir = ataca_para_direita(t) ? (CAMPO_X + CAMPO_W) : (CAMPO_X + AREA_PENALTI_W);
         float area_y_min = CAMPO_Y + CAMPO_H*0.5f - AREA_PENALTI_H*0.5f;
         float area_y_max = CAMPO_Y + CAMPO_H*0.5f + AREA_PENALTI_H*0.5f;
         int na_area = (p->x >= area_x_esq && p->x <= area_x_dir &&
                        p->y >= area_y_min && p->y <= area_y_max);
 
         /* Chuta se na area ou dentro do limite */
         if ((na_area || d_gol < DIST_CHUTE) && p->cd_chute <= 0) {
             float dvx = cx_gol - p->x, dvy = cy_gol - p->y;
             normalize(&dvx, &dvy);
             float ruido = (((float)rand()/RAND_MAX)-0.5f)*0.25f;
             dvx += ruido; dvy += ruido; normalize(&dvx, &dvy);
             bola.vx = dvx*VELOCIDADE_BOLA*1.1f;
             bola.vy = dvy*VELOCIDADE_BOLA*1.1f;
             bola.posse = -1; p->com_bola = 0; p->cd_chute = 1.0f;
             ultimo_toque = t;
             audio_play_kick();
         } else {
             /* Tenta passe aleatorio ou conduz para o gol */
             if (((float)rand()/RAND_MAX) < 0.018f) {
                 if (!tentar_passe(t, idx, -1)) {
                     float dvx = cx_gol-p->x, dvy = cy_gol-p->y;
                     normalize(&dvx, &dvy);
                     p->alvo_x = p->x + dvx*50; p->alvo_y = p->y + dvy*50;
                     mover_para_alvo(p, p->alvo_x, p->alvo_y, fv);
                 }
             } else {
                 float dvx = cx_gol-p->x, dvy = cy_gol-p->y;
                 normalize(&dvx, &dvy);
                 p->alvo_x = p->x + dvx*50; p->alvo_y = p->y + dvy*50;
                 mover_para_alvo(p, p->alvo_x, p->alvo_y, fv);
             }
         }
         aplicar_movimento(p, idx, t);
         grudar_bola(p, t);
         return;
     }
 
     /* Jogador mais próximo da bola no time */
     int mais_proximo = 0;
     {
         float dmin = 999999.f;
         for (j = 0; j < NUM_JOGADORES; j++) {
             float d = fdist(jogadores[t][j].x, jogadores[t][j].y, bola.x, bola.y);
             if (d < dmin) { dmin = d; mais_proximo = j; }
         }
     }
 
     float d_bola = fdist(p->x, p->y, bola.x, bola.y);
     float raio_ativo = (adv_tem) ? RAIO_PRESSAO_DEFESA : p->raio_pressao;
 
     /* ── BOLA LIVRE ── */
     if (bola.posse == -1) {
         if (idx == mais_proximo)
             { p->alvo_x = bola.x; p->alvo_y = bola.y; }
         else
             { p->alvo_x = p->base_x; p->alvo_y = p->base_y; }
 
     /* ── MEU TIME TEM A BOLA: posicao de suporte ── */
     } else if (meu_time_tem) {
         /* Todos abrem espaco em volta do portador respeitando zona */
         float angulo = ((float)idx / NUM_JOGADORES) * 2.f * (float)M_PI;
         float tx = jogadores[t][quem_meu].x + cosf(angulo)*65.f;
         float ty = jogadores[t][quem_meu].y + sinf(angulo)*65.f;
         tx = tx*0.35f + p->base_x*0.65f;
         ty = ty*0.35f + p->base_y*0.65f;
         if (ataca_para_direita(t)) tx = fclamp(tx, x_def, x_atk);
         else                       tx = fclamp(tx, x_atk, x_def);
         ty = fclamp(ty, CAMPO_Y+8.f, CAMPO_Y+CAMPO_H-8.f);
         p->alvo_x = tx; p->alvo_y = ty;
 
     /* ── ADVERSÁRIO TEM A BOLA ── */
     } else if (adv_tem) {
         Jogador *portador = &jogadores[adv][adv_quem];
 
         if (idx == mais_proximo) {
             /* Jogador mais proximo SEMPRE pressiona o portador */
             p->alvo_x = portador->x;
             p->alvo_y = portador->y;
             /* Tenta desarme com reducao pela fadiga */
             if (p->cd_tackle <= 0 &&
                 pode_desarmar_na_saida(t) &&
                 tackle_valido_posicao(p, portador, DIST_TACKLE * 1.35f)) {
                 float chance = ((float)rand()/RAND_MAX);
                 float fator = chance_tackle_fadiga(p);
                 if (chance < 0.32f * fator) {
                     /* Rouba com posse */
                     portador->com_bola = 0;
                     bola.posse = t * 100 + idx;
                     p->com_bola = 1;
                     bola.vx = 0; bola.vy = 0;
                     p->cd_tackle = 1.0f;
                 } else if (chance < 0.62f * fator) {
                     /* Bola fica livre */
                     portador->com_bola = 0;
                     bola.posse = -1;
                     float dvx = p->x - portador->x, dvy = p->y - portador->y;
                     normalize(&dvx, &dvy);
                     float ruido = (((float)rand()/RAND_MAX)-0.5f)*1.5f;
                     bola.vx = dvx*2.0f + ruido;
                     bola.vy = dvy*2.0f + ruido;
                     p->cd_tackle = 1.0f;
                 } else {
                     /* Falha no bote */
                     p->cd_tackle = 0.45f;
                 }
             }
         } else if (d_bola < raio_ativo) {
             /* Dentro do raio de pressao: fecha o portador por cobertura de zona */
             float meu_gol_x = x_gol_proprio(t);
             float meu_gol_y = CAMPO_Y + CAMPO_H * 0.5f;
             /* Posicao entre o adversario marcado e o gol proprio */
             float tx2 = portador->x * 0.60f + meu_gol_x * 0.40f;
             float ty2 = portador->y * 0.60f + meu_gol_y * 0.40f;
             tx2 = tx2 * 0.55f + p->base_x * 0.45f;
             ty2 = ty2 * 0.55f + p->base_y * 0.45f;
             if (ataca_para_direita(t)) tx2 = fclamp(tx2, x_def, x_atk);
             else                       tx2 = fclamp(tx2, x_atk, x_def);
             ty2 = fclamp(ty2, CAMPO_Y+8.f, CAMPO_Y+CAMPO_H-8.f);
             p->alvo_x = tx2; p->alvo_y = ty2;
         } else {
             /* Fora do raio: mantem posicao defensiva na zona */
             pos_defensiva(t, idx, 0.45f);
             if (ataca_para_direita(t)) p->alvo_x = fclamp(p->alvo_x, x_def, x_atk);
             else                       p->alvo_x = fclamp(p->alvo_x, x_atk, x_def);
         }
     } else {
         p->alvo_x = p->base_x; p->alvo_y = p->base_y;
     }
 
     mover_para_alvo(p, p->alvo_x, p->alvo_y, fv);
     aplicar_movimento(p, idx, t);
 
     if (bola.posse == -1 && pode_pegar_bola(t, idx)) {
         float db = fdist(p->x, p->y, bola.x, bola.y);
         if (db < RAIO_JOGADOR + RAIO_BOLA + 2) {
             bola.posse = t*100+idx; p->com_bola = 1;
             bola.vx = 0; bola.vy = 0;
             ultimo_toque = t;
         }
     }
     if (p->com_bola) grudar_bola(p, t);
 }
 
 /* ─────────────────────────────────────────
    ATRIBUIÇÃO DE MARCAÇÃO (GERADO POR IA)
 ───────────────────────────────────────── */
 static void atribuir_marcacoes(int t) {
     int adv = 1 - t;
     int hum = jogador_humano[t];
     int j, k;
 
     /* Zera marcações de todos os companheiros */
     for (j = 0; j < NUM_JOGADORES; j++) {
         if (j == hum) continue;
         jogadores[t][j].marcando = -1;
     }
 
     /* Para cada companheiro (exceto goleiro e humano), encontra o
        adversário mais próximo ainda não marcado por ninguém */
     for (j = 1; j < NUM_JOGADORES; j++) {   /* j=1 pula goleiro */
         if (j == hum) continue;
         Jogador *p = &jogadores[t][j];
         if (p->com_bola) continue;           /* quem tem bola não marca */
 
         float dmin = 999999.f;
         int   alvo = -1;
         for (k = 0; k < NUM_JOGADORES; k++) {
             /* Adversário já marcado por outro companheiro? pula */
             int ja_marcado = 0;
             int m;
             for (m = 0; m < NUM_JOGADORES; m++) {
                 if (m == j || m == hum) continue;
                 if (jogadores[t][m].marcando == k) { ja_marcado = 1; break; }
             }
             if (ja_marcado) continue;
 
             float d = fdist(p->x, p->y,
                             jogadores[adv][k].x, jogadores[adv][k].y);
             /* Só marca adversário dentro de raio razoável da sua zona */
             if (d < dmin && d < DIST_MARCACAO * 2.2f) {
                 dmin = d; alvo = k;
             }
         }
         p->marcando = alvo;   /* -1 se nenhum adversário próximo */
     }
 
     /* Goleiro (idx=0) sempre "marca" o portador ou o adversário
        mais perigoso perto do gol — lógica simples */
     if (0 != hum) {
         Jogador *gol = &jogadores[t][0];
         float meu_gol_x = x_gol_proprio(t);
         float dmin = 999999.f; int alvo = -1;
         for (k = 0; k < NUM_JOGADORES; k++) {
             float d = fdist(meu_gol_x, CAMPO_Y+CAMPO_H*0.5f,
                             jogadores[adv][k].x, jogadores[adv][k].y);
             if (d < dmin) { dmin = d; alvo = k; }
         }
         gol->marcando = alvo;
     }
 }
 
 /* ─────────────────────────────────────────
    IA do player (gerado por IA)
 ───────────────────────────────────────── */
 static void ia_companheiro(int t, int idx) {
     Jogador *p = &jogadores[t][idx];
     int adv = 1 - t;
     int hum = jogador_humano[t];
     int j;
 
     if (reposicao_tipo != REPOSICAO_NENHUMA && reposicao_time == t) {
         float alvo_x = reposicao_bola_x, alvo_y = reposicao_bola_y;
         alvo_cobrador_reposicao(&alvo_x, &alvo_y);
         if (idx == reposicao_jogador) {
             mover_para_alvo(p, alvo_x, alvo_y, 1.0f);
         } else {
             p->alvo_x = p->base_x;
             p->alvo_y = p->base_y;
             mover_para_alvo(p, p->alvo_x, p->alvo_y, 1.0f);
         }
         aplicar_movimento(p, idx, t);
         return;
     }
 
     if (p->cd_tackle > 0) p->cd_tackle -= 0.016f;
     if (p->cd_passe  > 0) p->cd_passe  -= 0.016f;
     if (p->cd_chute  > 0) p->cd_chute  -= 0.016f;
 
     /* Fadiga */
     atualizar_fadiga(p);
     float fv = fator_vel_fadiga(p);
 
     float cx_gol = x_gol_adversario(t);
     float cy_gol = CAMPO_Y + CAMPO_H*0.5f;
     float x_def, x_atk;
     zona_jogador(t, idx, &x_def, &x_atk);
 
     /* ── COM A BOLA: chuta ou toca para o humano ── */
     if (p->com_bola) {
         float d_gol = fdist(p->x, p->y, cx_gol, cy_gol);
 
         /* Verifica área do pênalti */
         float area_x_esq = ataca_para_direita(t) ? (CAMPO_X + CAMPO_W - AREA_PENALTI_W) : CAMPO_X;
         float area_x_dir = ataca_para_direita(t) ? (CAMPO_X + CAMPO_W) : (CAMPO_X + AREA_PENALTI_W);
         float area_y_min = CAMPO_Y + CAMPO_H*0.5f - AREA_PENALTI_H*0.5f;
         float area_y_max = CAMPO_Y + CAMPO_H*0.5f + AREA_PENALTI_H*0.5f;
         int na_area = (p->x >= area_x_esq && p->x <= area_x_dir &&
                        p->y >= area_y_min && p->y <= area_y_max);
 
         if ((na_area || d_gol < DIST_CHUTE) && p->cd_chute <= 0) {
             float dvx = cx_gol-p->x, dvy = cy_gol-p->y;
             normalize(&dvx, &dvy);
             float ruido = (((float)rand()/RAND_MAX)-0.5f)*0.25f;
             dvx += ruido; dvy += ruido; normalize(&dvx, &dvy);
             bola.vx = dvx*VELOCIDADE_BOLA*1.1f;
             bola.vy = dvy*VELOCIDADE_BOLA*1.1f;
             bola.posse = -1; p->com_bola = 0; p->cd_chute = 1.f;
             ultimo_toque = t;
             audio_play_kick();
         } else {
             /* Tenta passar para o humano primeiro */
             if (!tentar_passe(t, idx, hum)) {
                 float dvx = cx_gol-p->x, dvy = cy_gol-p->y;
                 normalize(&dvx, &dvy);
                 p->alvo_x = p->x + dvx*50; p->alvo_y = p->y + dvy*50;
                 mover_para_alvo(p, p->alvo_x, p->alvo_y, fv);
             }
         }
         aplicar_movimento(p, idx, t);
         grudar_bola(p, t);
         return;
     }
 
     /* Situação da bola */
     int meu_time_tem = 0, quem_meu = -1;
     int adv_tem = 0;
     for (j = 0; j < NUM_JOGADORES; j++) {
         if (jogadores[t][j].com_bola)   { meu_time_tem = 1; quem_meu = j; }
         if (jogadores[adv][j].com_bola) adv_tem = 1;
     }
 
     /* Mais próximo da bola no meu time (excluindo humano) */
     int mais_proximo = -1;
     {
         float dmin = 999999.f;
         for (j = 0; j < NUM_JOGADORES; j++) {
             if (j == hum) continue;
             float d = fdist(jogadores[t][j].x, jogadores[t][j].y, bola.x, bola.y);
             if (d < dmin) { dmin = d; mais_proximo = j; }
         }
     }
     float d_hum = (hum >= 0) ?
         fdist(jogadores[t][hum].x, jogadores[t][hum].y, bola.x, bola.y) : 999999.f;
 
     /* ── BOLA LIVRE: o mais próximo vai buscar ── */
     if (bola.posse == -1) {
         float d_eu = fdist(p->x, p->y, bola.x, bola.y);
         if (idx == mais_proximo && d_eu < d_hum) {
             p->alvo_x = bola.x; p->alvo_y = bola.y;
         } else {
             /* Outros: posição de apoio respeitando zona */
             p->alvo_x = p->base_x; p->alvo_y = p->base_y;
         }
 
     /* ── MEU TIME COM A BOLA: abre espaço ── */
     } else if (meu_time_tem) {
         float angulo = ((float)idx / NUM_JOGADORES) * 2.f * (float)M_PI;
         float tx = jogadores[t][quem_meu].x + cosf(angulo)*70.f;
         float ty = jogadores[t][quem_meu].y + sinf(angulo)*70.f;
         tx = tx*0.40f + p->base_x*0.60f;
         ty = ty*0.40f + p->base_y*0.60f;
         if (ataca_para_direita(t)) tx = fclamp(tx, x_def, x_atk);
         else                       tx = fclamp(tx, x_atk, x_def);
         ty = fclamp(ty, CAMPO_Y+8.f, CAMPO_Y+CAMPO_H-8.f);
         p->alvo_x = tx; p->alvo_y = ty;
 
     /* ── ADVERSÁRIO COM A BOLA: MARCAR ── */
     } else if (adv_tem || bola.posse == -1) {
         int alvo_marc = p->marcando; /* atribuído por atribuir_marcacoes() */
 
         if (alvo_marc >= 0) {
             Jogador *adv_j = &jogadores[adv][alvo_marc];
 
             if (adv_j->com_bola) {
                 
                 p->alvo_x = adv_j->x;
                 p->alvo_y = adv_j->y;
                 if (tentar_tackle(p, t, idx, adv, alvo_marc)) {
                     /* Roubou! Passa controle ao humano */
                     jogador_humano[t] = idx;
                     goto move_fim;
                 }
             } else {
                 
                 float meu_gol_x = x_gol_proprio(t);
                 float meu_gol_y = CAMPO_Y + CAMPO_H * 0.5f;
                 /* Posição entre adversário e gol (60% adv, 40% gol) */
                 float tx2 = adv_j->x * 0.65f + meu_gol_x * 0.35f;
                 float ty2 = adv_j->y * 0.65f + meu_gol_y * 0.35f;
                 /* Mistura com base para não sair muito da zona */
                 tx2 = tx2 * 0.6f + p->base_x * 0.4f;
                 ty2 = ty2 * 0.6f + p->base_y * 0.4f;
                 /* Clampeia na zona */
                 if (ataca_para_direita(t)) tx2 = fclamp(tx2, x_def, x_atk);
                 else                       tx2 = fclamp(tx2, x_atk, x_def);
                 ty2 = fclamp(ty2, CAMPO_Y+8.f, CAMPO_Y+CAMPO_H-8.f);
                 p->alvo_x = tx2;
                 p->alvo_y = ty2;
             }
         } else {
             
             pos_defensiva(t, idx, 0.45f);
             if (ataca_para_direita(t)) p->alvo_x = fclamp(p->alvo_x, x_def, x_atk);
             else                       p->alvo_x = fclamp(p->alvo_x, x_atk, x_def);
         }
 
     } else {
         p->alvo_x = p->base_x; p->alvo_y = p->base_y;
     }
 
 move_fim:
     mover_para_alvo(p, p->alvo_x, p->alvo_y, fv);
     aplicar_movimento(p, idx, t);
 
     /* Pegar bola livre — ao pegar, passa controle ao humano */
     if (bola.posse == -1 && pode_pegar_bola(t, idx)) {
         float db = fdist(p->x, p->y, bola.x, bola.y);
         if (db < RAIO_JOGADOR + RAIO_BOLA + 2) {
             bola.posse = t*100+idx; p->com_bola = 1;
             bola.vx = 0; bola.vy = 0;
             ultimo_toque = t;
             jogador_humano[t] = idx;   /* assume o controle */
         }
     }
     if (p->com_bola) grudar_bola(p, t);
 }
 
 /* ─────────────────────────────────────────
    CONTROLE HUMANO
 ───────────────────────────────────────── */
 static void controle_humano(int time_id) {
     int idx = jogador_humano[time_id];
     if (idx < 0 || idx >= NUM_JOGADORES) return;
     Jogador *p = &jogadores[time_id][idx];
 
     /* ── COBRANÇA DE LATERAL / ESCANTEIO pelo boneco humano ── */
     if (reposicao_tipo != REPOSICAO_NENHUMA && reposicao_time == time_id &&
         (reposicao_tipo != TIRO_META || idx == reposicao_jogador)) {
        limite_vel_humano[time_id] = VELOCIDADE_MAX;
         float alvo_x = reposicao_bola_x, alvo_y = reposicao_bola_y;
         alvo_cobrador_reposicao(&alvo_x, &alvo_y);
         float db = fdist(p->x, p->y, alvo_x, alvo_y);
         if (db > RAIO_JOGADOR + RAIO_BOLA + 3.f) {
             /* Ainda nao chegou: anda automaticamente ate a bola */
             mover_para_alvo(p, alvo_x, alvo_y, 1.0f);
            aplicar_movimento(p, idx, time_id);
         } else {
             /* Chegou: congela na posicao e deixa o jogador decidir */
             p->x = alvo_x; p->y = alvo_y;
             p->vx = 0; p->vy = 0;
         }
         return; /* controles normais bloqueados durante a cobranca */
     }
 
     /* ── SPRINT ── */
     atualizar_fadiga(p);
 
     int sprint_pressionado = (time_id == 0) ? (teclas['c']||teclas['C'])
                                              : (teclas['r']||teclas['R']);
     int sprint_ativo = (sprint_pressionado && p->energia > ENERGIA_MIN_SPRINT);
    float vel = sprint_ativo ? VELOCIDADE_SPRINT : VELOCIDADE_MAX * 1.22f;
     vel *= fator_vel_fadiga(p);
    limite_vel_humano[time_id] = vel;
     if (sprint_ativo) {
         p->energia -= CUSTO_SPRINT;
         if (p->energia < 0.f) p->energia = 0.f;
     }
 
     float mvx = 0, mvy = 0;
 
     if (time_id == 0) {
         if (teclas_esp[GLUT_KEY_LEFT])  mvx -= vel;
         if (teclas_esp[GLUT_KEY_RIGHT]) mvx += vel;
         if (teclas_esp[GLUT_KEY_UP])    mvy += vel;
         if (teclas_esp[GLUT_KEY_DOWN])  mvy -= vel;
     } else {
         if (teclas['a']||teclas['A']) mvx -= vel;
         if (teclas['d']||teclas['D']) mvx += vel;
         if (teclas['w']||teclas['W']) mvy += vel;
         if (teclas['s']||teclas['S']) mvy -= vel;
     }
     if (mvx != 0 && mvy != 0) { mvx *= 0.707f; mvy *= 0.707f; }
 
     p->vx = mvx; p->vy = mvy;
     aplicar_movimento(p, idx, time_id);
 
     /* Pegar bola */
     if (bola.posse == -1 && pode_pegar_bola(time_id, idx)) {
         float db = fdist(p->x, p->y, bola.x, bola.y);
         if (db < RAIO_JOGADOR + RAIO_BOLA + 2) {
             bola.posse = time_id*100+idx; p->com_bola = 1;
             bola.vx = 0; bola.vy = 0;
             ultimo_toque = time_id;
         }
     }
 
     /* Bola gruda */
     if (p->com_bola) {
         float spd = sqrtf(p->vx*p->vx+p->vy*p->vy);
         float ang = atan2f(p->vy, p->vx);
         if (spd < 0.1f) ang = ataca_para_direita(time_id) ? 0.f : (float)M_PI;
         bola.x = p->x + cosf(ang)*(RAIO_JOGADOR+RAIO_BOLA+1);
         bola.y = p->y + sinf(ang)*(RAIO_JOGADOR+RAIO_BOLA+1);
     }
 
     /* ── CHUTE ── */
     int chute = (time_id==0) ? teclas[' '] : (teclas['f']||teclas['F']);
     if (chute && p->com_bola && p->cd_chute <= 0) {
         float cx_gol = x_gol_adversario(time_id);
         float cy_gol = CAMPO_Y + CAMPO_H*0.5f;
         float dvx = cx_gol-p->x, dvy = cy_gol-p->y;
         float spd = sqrtf(p->vx*p->vx+p->vy*p->vy);
         if (spd > 0.5f) { dvx = p->vx; dvy = p->vy; }
         normalize(&dvx, &dvy);
         bola.vx = dvx*VELOCIDADE_BOLA*1.5f;
         bola.vy = dvy*VELOCIDADE_BOLA*1.5f;
         bola.posse = -1; p->com_bola = 0; p->cd_chute = 0.5f;
         ultimo_toque = time_id;
         audio_play_kick();
     }
     if (p->cd_chute > 0) p->cd_chute -= 0.016f;
 
     /* ── PASSE HUMANO ── Z (J1) ou H (J2) ── */
     int passe_key = (time_id==0) ? (teclas['z']||teclas['Z'])
                                   : (teclas['h']||teclas['H']);
     if (passe_key && p->com_bola && p->cd_passe <= 0) {
         int receptor = melhor_receptor_humano(time_id, idx);
         if (receptor >= 0) {
             Jogador *dest = &jogadores[time_id][receptor];
             float dvx = dest->x - p->x, dvy = dest->y - p->y;
             float dist_r = sqrtf(dvx*dvx + dvy*dvy);
             normalize(&dvx, &dvy);
             float vel_passe = VELOCIDADE_BOLA * (0.7f + 0.3f*(dist_r / (DIST_PASSE*1.4f)));
             bola.vx = dvx * vel_passe;
             bola.vy = dvy * vel_passe;
             bola.posse = -1;
             p->com_bola = 0;
             p->cd_passe = 0.6f;
             ultimo_toque = time_id;
 
             /* Indicador visual de passe */
             passe_visual_timer[time_id] = 35;
             passe_visual_x[time_id] = dest->x;
             passe_visual_y[time_id] = dest->y;
 
             /* Passa o controle para o receptor */
             jogador_humano[time_id] = receptor;
         }
     }
     if (p->cd_passe > 0) p->cd_passe -= 0.016f;
 
     /* ── TACKLE ── X (J1) ou G (J2) ── */
     int tackle = (time_id==0) ? (teclas['x']||teclas['X'])
                                : (teclas['g']||teclas['G']);
     if (tackle && p->cd_tackle <= 0 && pode_desarmar_na_saida(time_id)) {
         int adv = 1 - time_id;
         int j;
         for (j = 0; j < NUM_JOGADORES; j++) {
             if (!jogadores[adv][j].com_bola) continue;
             float dd = fdist(p->x, p->y, jogadores[adv][j].x, jogadores[adv][j].y);
             if (dd < DIST_TACKLE * 2.0f) {
                 float chance = ((float)rand()/RAND_MAX);
                 if (chance < chance_tackle_fadiga(p)) {
                     jogadores[adv][j].com_bola = 0;
                     /* Jogador humano assume posse se acertar o desarme */
                     bola.posse = time_id * 100 + idx;
                     p->com_bola = 1;
                     bola.vx = 0; bola.vy = 0;
                     ultimo_toque = time_id;
                 }
                 p->cd_tackle = 1.f;
                 break;
             }
         }
     }
     if (p->cd_tackle > 0) p->cd_tackle -= 0.016f;
 }
 
 /* ─────────────────────────────────────────
    FÍSICA DA BOLA + LATERAL/ESCANTEIO
 ───────────────────────────────────────── */
 static void atualiza_bola(void) {
     if (bola.posse != -1) {
         int tt   = bola.posse / 100;
         int tidx = bola.posse % 100;
 
         /* Mantem ultimo toque coerente durante a conducao */
         if (tt == 0 || tt == 1) ultimo_toque = tt;
 
         /* Se a bola colada ao jogador saiu, solta e processa reposicao */
         if (bola.x - RAIO_BOLA < CAMPO_X || bola.x + RAIO_BOLA > CAMPO_X + CAMPO_W ||
             bola.y - RAIO_BOLA < CAMPO_Y || bola.y + RAIO_BOLA > CAMPO_Y + CAMPO_H) {
             if (tt >= 0 && tt <= 1 && tidx >= 0 && tidx < NUM_JOGADORES) {
                 jogadores[tt][tidx].com_bola = 0;
             }
             bola.posse = -1;
             bola.vx = 0.f;
             bola.vy = 0.f;
             /* Continua para o processamento de saida abaixo */
         } else {
             return;
         }
     }
 
     /* ── Processa reposição (lateral ou escanteio) ── */
     if (reposicao_tipo != REPOSICAO_NENHUMA) {
         bola.x  = reposicao_bola_x;
         bola.y  = reposicao_bola_y;
         bola.vx = 0; bola.vy = 0;
 
        if (reposicao_apito_timer > 0) {
            reposicao_apito_timer--;
            if (reposicao_apito_timer == 0) audio_play_apito_curto();
            return;
        }

         if (reposicao_jogador < 0 || reposicao_jogador >= NUM_JOGADORES) {
             reposicao_jogador = (reposicao_tipo == TIRO_META)
                                 ? 0
                                 : escolher_cobrador_reposicao(reposicao_time);
         }
 
         /* Se for o time humano cobrar: timer nao conta — o jogador
            chega ate a bola e pressiona chute/passe para executar.
            A cobrança é disparada via controle_humano quando o boneco
            alcança a bola e o jogador aperta o botão. */
         int humano_no_time = (reposicao_time == 0 || modo_jogo == MODO_2P);
         int humano_eh_cobrador = humano_no_time &&
                                  reposicao_jogador >= 0 &&
                                  jogador_humano[reposicao_time] == reposicao_jogador;
         int time_humano_cobra = ((reposicao_tipo != TIRO_META) && humano_no_time) ||
                                 ((reposicao_tipo == TIRO_META) && humano_eh_cobrador);
 
         if (time_humano_cobra && reposicao_tipo != TIRO_META && reposicao_jogador >= 0) {
             jogador_humano[reposicao_time] = reposicao_jogador;
         }
         if (time_humano_cobra) {
             /* Verifica se o boneco humano chegou e pressionou o botao de chute/passe */
             int tidx = reposicao_jogador;
             Jogador *ph = &jogadores[reposicao_time][tidx];
             float db = fdist(ph->x, ph->y, reposicao_bola_x, reposicao_bola_y);
             int chegou = (db < RAIO_JOGADOR + RAIO_BOLA + 5.f);
             int btn_chute = (reposicao_time==0) ? teclas[' '] : (teclas['f']||teclas['F']);
             int btn_passe = (reposicao_time==0) ? (teclas['z']||teclas['Z'])
                                                 : (teclas['h']||teclas['H']);
             int cobrar_auto = 0;
 
             /* Escanteio humano: contador máximo de 10 segundos para decidir */
             if (reposicao_tipo == ESCANTEIO) {
                 if (reposicao_timer_humano <= 0) reposicao_timer_humano = 600; /* 10s @60fps */
                reposicao_timer_humano--;
                if (reposicao_timer_humano <= 0) cobrar_auto = 1;
             } else {
                 reposicao_timer_humano = 0;
             }
 
            /* Segurança: se o timer venceu e o humano ainda não encostou,
               posiciona o cobrador automaticamente para evitar travar jogada. */
            if (cobrar_auto && !chegou) {
                ph->x = reposicao_bola_x;
                ph->y = reposicao_bola_y;
                ph->vx = ph->vy = 0.f;
                chegou = 1;
            }

             if (chegou && (btn_chute || btn_passe || cobrar_auto)) {
                 /* Coloca bola na posse do boneco e dispara o kick */
                 ph->com_bola = 1;
                 bola.posse = reposicao_time * 100 + tidx;
                 float cx_gol = x_gol_adversario(reposicao_time);
                 float cy_gol = CAMPO_Y + CAMPO_H*0.5f;
                 if (btn_chute) {
                     /* Chute direto para o gol (ou para onde o boneco aponta) */
                     float dvx = cx_gol - ph->x, dvy = cy_gol - ph->y;
                     float spd = sqrtf(ph->vx*ph->vx + ph->vy*ph->vy);
                     if (spd > 0.3f) { dvx = ph->vx; dvy = ph->vy; }
                     normalize(&dvx, &dvy);
                     bola.vx = dvx * VELOCIDADE_BOLA * 1.3f;
                     bola.vy = dvy * VELOCIDADE_BOLA * 1.3f;
                     bola.posse = -1; ph->com_bola = 0;
                     ultimo_toque = reposicao_time;
                     audio_play_kick();
                 } else {
                     /* Passe para o melhor companheiro */
                     int receptor = melhor_receptor_humano(reposicao_time, tidx);
                     if (receptor >= 0) {
                         Jogador *dest = &jogadores[reposicao_time][receptor];
                         float dvx = dest->x - ph->x, dvy = dest->y - ph->y;
                         normalize(&dvx, &dvy);
                         bola.vx = dvx * VELOCIDADE_BOLA * 0.9f;
                         bola.vy = dvy * VELOCIDADE_BOLA * 0.9f;
                         bola.posse = -1; ph->com_bola = 0;
                         ultimo_toque = reposicao_time;
                         passe_visual_timer[reposicao_time] = 35;
                         passe_visual_x[reposicao_time] = dest->x;
                         passe_visual_y[reposicao_time] = dest->y;
                         jogador_humano[reposicao_time] = receptor;
                     } else {
                         /* Sem receptor válido: cruza para área como fallback */
                         float alvo_x = ataca_para_direita(reposicao_time)
                                        ? (CAMPO_X + CAMPO_W - 55.f)
                                        : (CAMPO_X + 55.f);
                         float alvo_y = cy_gol + (((float)rand()/RAND_MAX)-0.5f) * GOL_H * 0.9f;
                         float dvx = alvo_x - ph->x, dvy = alvo_y - ph->y;
                         normalize(&dvx, &dvy);
                         bola.vx = dvx * VELOCIDADE_BOLA * 1.05f;
                         bola.vy = dvy * VELOCIDADE_BOLA * 1.05f;
                         bola.posse = -1; ph->com_bola = 0;
                         ultimo_toque = reposicao_time;
                     }
                 }
                 reposicao_tipo = REPOSICAO_NENHUMA;
                 reposicao_time = -1;
                 reposicao_jogador = -1;
                 reposicao_timer_humano = 0;
                reposicao_apito_timer = 0;
             }
             return;
         }
 
         /* IA cobra: conta o timer normalmente */
         reposicao_timer--;
         if (reposicao_timer <= 0) {
             int pode_cobrar_agora = adversarios_longe_reposicao();
             /* Anti-loop: apos esperar bastante, cobra mesmo com adversario perto */
             if (!pode_cobrar_agora && reposicao_timer > -180) return;
 
             int t   = reposicao_time;
             int adv = 1 - t;
             int melhor = reposicao_jogador;
             if (melhor < 0 || melhor >= NUM_JOGADORES) melhor = escolher_cobrador_reposicao(t);
             if (melhor < 0 || melhor >= NUM_JOGADORES) return;
             {
                 float dchegou = fdist(jogadores[t][melhor].x, jogadores[t][melhor].y,
                                       reposicao_bola_x, reposicao_bola_y);
                 if (dchegou > RAIO_JOGADOR + RAIO_BOLA + 6.f) return;
             }
             int j;
             Jogador *exe = &jogadores[t][melhor];
 
             if (reposicao_tipo == LATERAL || reposicao_tipo == TIRO_META) {
                 /* Lateral/tiro de meta: bola ja esta dentro do campo (reposicao_bola_x/y corretos) */
                 float cx_gol = x_gol_adversario(t);
                 float cy_gol = CAMPO_Y + CAMPO_H*0.5f;
                 /* Melhor companheiro disponível */
                 int dest = -1; float smax = -999999.f;
                 for (j = 0; j < NUM_JOGADORES; j++) {
                     if (j == melhor) continue;
                     float dc = fdist(exe->x, exe->y,
                                      jogadores[t][j].x, jogadores[t][j].y);
                     if (dc > DIST_PASSE * 1.5f || dc < 15.f) continue;
                     float av = progresso_ataque(t, jogadores[t][j].x);
                     float pressao = 999.f;
                     int k;
                     for (k = 0; k < NUM_JOGADORES; k++) {
                         float dd = fdist(jogadores[t][j].x, jogadores[t][j].y,
                                          jogadores[adv][k].x, jogadores[adv][k].y);
                         if (dd < pressao) pressao = dd;
                     }
                     float s = av + pressao*0.3f - dc*0.15f;
                     if (s > smax) { smax = s; dest = j; }
                 }
                 if (dest >= 0) {
                     float dvx = jogadores[t][dest].x - bola.x;
                     float dvy = jogadores[t][dest].y - bola.y;
                     normalize(&dvx, &dvy);
                     float mult = (reposicao_tipo == TIRO_META) ? 1.0f : 0.85f;
                     bola.vx = dvx * VELOCIDADE_BOLA * mult;
                     bola.vy = dvy * VELOCIDADE_BOLA * mult;
                 } else {
                     /* Sem receptor: joga para frente */
                     float dvx = cx_gol - bola.x, dvy = cy_gol - bola.y;
                     normalize(&dvx, &dvy);
                     float mult = (reposicao_tipo == TIRO_META) ? 0.9f : 0.7f;
                     bola.vx = dvx * VELOCIDADE_BOLA * mult;
                     bola.vy = dvy * VELOCIDADE_BOLA * mult;
                 }
             } else { /* ESCANTEIO */
                 /* Escanteio: cruza para a area do gol adversario.
                    reposicao_bola_x/y ja estao 6px dentro do campo (sem loop). */
                 float cx_gol_x = ataca_para_direita(t) ? (CAMPO_X + CAMPO_W) : CAMPO_X;
                 float cy_gol   = CAMPO_Y + CAMPO_H*0.5f;
                 /* Alvo: dentro da grande area com variacao vertical */
                 float alvo_x = cx_gol_x + (ataca_para_direita(t) ? -60.f : 60.f);
                 float alvo_y = cy_gol + (((float)rand()/RAND_MAX)-0.5f) * GOL_H * 0.9f;
                 float dvx = alvo_x - bola.x, dvy = alvo_y - bola.y;
                 normalize(&dvx, &dvy);
                 bola.vx = dvx * VELOCIDADE_BOLA * 1.1f;
                 bola.vy = dvy * VELOCIDADE_BOLA * 1.1f;
                 /* Mantem o ultimo toque no cobrador para evitar lateral invertida */
                 ultimo_toque = t;
             }
             if (reposicao_tipo == LATERAL || reposicao_tipo == TIRO_META) ultimo_toque = t;
             audio_play_kick();
             reposicao_tipo   = REPOSICAO_NENHUMA;
             reposicao_time   = -1;
             reposicao_jogador = -1;
             reposicao_timer_humano = 0;
            reposicao_apito_timer = 0;
         }
         return;
     }
 
     bola.x += bola.vx;
     bola.y += bola.vy;
     bola.vx *= ATRITO_BOLA;
     bola.vy *= ATRITO_BOLA;
 
     /* Chute perigoso: bola rápida aproximando do gol (com cooldown) */
     if (bola.posse == -1 && ultimo_toque >= 0 && ultimo_toque <= 1 &&
         audio_danger_cd <= 0 && reposicao_tipo == REPOSICAO_NENHUMA) {
         float vm = sqrtf(bola.vx * bola.vx + bola.vy * bola.vy);
         if (vm > 2.05f) {
             int u = ultimo_toque;
             float cy0 = CAMPO_Y + CAMPO_H * 0.5f;
             float d_linha;
             int ar = ataca_para_direita(u);
             if (ar) d_linha = (CAMPO_X + CAMPO_W) - bola.x;
             else     d_linha = bola.x - CAMPO_X;
             int rumo = ar ? (bola.vx > 0.38f) : (bola.vx < -0.38f);
             if (rumo && d_linha < 200.f && d_linha > -12.f &&
                 fabsf(bola.y - cy0) < 125.f) {
                 audio_play_perigo();
                 audio_danger_cd = 110;
             }
         }
     }
 
     float esq   = CAMPO_X;
     float dir   = CAMPO_X + CAMPO_W;
     float base  = CAMPO_Y;
     float topo  = CAMPO_Y + CAMPO_H;
 
     float gol_min = CAMPO_Y + CAMPO_H * 0.5f - GOL_H * 0.5f;
     float gol_max = CAMPO_Y + CAMPO_H * 0.5f + GOL_H * 0.5f;
 
     /* ── LATERAL: bola saiu pelo topo ou base da linha lateral ── */
     if (bola.y - RAIO_BOLA < base || bola.y + RAIO_BOLA > topo) {
         /* Adversario de quem tocou por ultimo cobra a lateral */
         reposicao_time  = (ultimo_toque == 0) ? 1 : 0;
         reposicao_tipo  = LATERAL;
        reposicao_timer = 1;
         reposicao_timer_humano = 0;
        reposicao_apito_timer = 180;
         /* X = onde saiu (clampado para nao ficar nos cantos extremos)
            Y = exatamente na linha lateral, 5px dentro para nao re-acionar */
         reposicao_bola_x = fclamp(bola.x, esq + 20.f, dir - 20.f);
         reposicao_bola_y = (bola.y - RAIO_BOLA < base)
                            ? base + RAIO_BOLA + 5.f
                            : topo - RAIO_BOLA - 5.f;
         bola.x = reposicao_bola_x;
         bola.y = reposicao_bola_y;
         bola.vx = 0; bola.vy = 0;
         reposicao_jogador = escolher_cobrador_reposicao(reposicao_time);
         return;
     }
 
     /* ── LINHA DE FUNDO ESQUERDA ── */
     if (bola.x - RAIO_BOLA < esq) {
         int defensor_esq = ataca_para_direita(0) ? 0 : 1;
         int atacante_esq = 1 - defensor_esq;
         if (bola.y >= gol_min && bola.y <= gol_max) {
             /* GOL do time que ataca para a esquerda */
             placar[atacante_esq]++;
             ultimo_gol = atacante_esq;
             estado     = GOL_MARCADO;
            tempo_gol  = TEMPO_GOL_FRAMES;
             audio_play_gol();
             gol_crowd_boost = 260;
         } else {
             /* Defensor do lado esquerdo*/
             if (ultimo_toque == atacante_esq) {
                 reposicao_time   = defensor_esq;
                 reposicao_tipo   = TIRO_META;
                reposicao_timer  = 1;
                 reposicao_timer_humano = 0;
                reposicao_apito_timer = 180;
                 reposicao_bola_x = esq + 35.f;
                 reposicao_bola_y = CAMPO_Y + CAMPO_H*0.5f;
             } else {
                 reposicao_time   = atacante_esq;
                 reposicao_tipo   = ESCANTEIO;
                reposicao_timer  = 1;
                 reposicao_timer_humano = 600;
                reposicao_apito_timer = 180;
                 /* Canto do campo*/
                 reposicao_bola_x = esq + RAIO_BOLA + 6.f;
                 reposicao_bola_y = (bola.y < CAMPO_Y + CAMPO_H*0.5f)
                                    ? base + RAIO_BOLA + 6.f
                                    : topo - RAIO_BOLA - 6.f;
             }
             bola.x = reposicao_bola_x;
             bola.y = reposicao_bola_y;
             bola.vx = 0; bola.vy = 0;
             reposicao_jogador = (reposicao_tipo == TIRO_META)
                                 ? 0
                                 : escolher_cobrador_reposicao(reposicao_time);
         }
         return;
     }
 
     /* ── LINHA DE FUNDO DIREITA ── */
     if (bola.x + RAIO_BOLA > dir) {
         int defensor_dir = ataca_para_direita(0) ? 1 : 0;
         int atacante_dir = 1 - defensor_dir;
         if (bola.y >= gol_min && bola.y <= gol_max) {
             /* GOL do time que ataca para a direita */
             placar[atacante_dir]++;
             ultimo_gol = atacante_dir;
             estado     = GOL_MARCADO;
            tempo_gol  = TEMPO_GOL_FRAMES;
             audio_play_gol();
             gol_crowd_boost = 260;
         } else {
             if (ultimo_toque == atacante_dir) {
                 reposicao_time   = defensor_dir;
                 reposicao_tipo   = TIRO_META;
                reposicao_timer  = 1;
                 reposicao_timer_humano = 0;
                reposicao_apito_timer = 180;
                 reposicao_bola_x = dir - 35.f;
                 reposicao_bola_y = CAMPO_Y + CAMPO_H*0.5f;
             } else {
                 reposicao_time   = atacante_dir;
                 reposicao_tipo   = ESCANTEIO;
                reposicao_timer  = 1;
                 reposicao_timer_humano = 600;
                reposicao_apito_timer = 180;
                 reposicao_bola_x = dir - RAIO_BOLA - 6.f;
                 reposicao_bola_y = (bola.y < CAMPO_Y + CAMPO_H*0.5f)
                                    ? base + RAIO_BOLA + 6.f
                                    : topo - RAIO_BOLA - 6.f;
             }
             bola.x = reposicao_bola_x;
             bola.y = reposicao_bola_y;
             bola.vx = 0; bola.vy = 0;
             reposicao_jogador = (reposicao_tipo == TIRO_META)
                                 ? 0
                                 : escolher_cobrador_reposicao(reposicao_time);
         }
         return;
     }
 }
 
 /* ─────────────────────────────────────────
    DESENHO DO CAMPO
 ───────────────────────────────────────── */
 static void draw_campo(void) {
     float g1r=0.15f,g1g=0.55f,g1b=0.15f;
     float g2r=0.13f,g2g=0.50f,g2b=0.13f;
     float fw = CAMPO_W / 10.f;
     int i;
     for (i = 0; i < 10; i++) {
         if (i%2==0) glColor3f(g1r,g1g,g1b); else glColor3f(g2r,g2g,g2b);
         glBegin(GL_QUADS);
         glVertex2f(CAMPO_X+i*fw,     CAMPO_Y);
         glVertex2f(CAMPO_X+(i+1)*fw, CAMPO_Y);
         glVertex2f(CAMPO_X+(i+1)*fw, CAMPO_Y+CAMPO_H);
         glVertex2f(CAMPO_X+i*fw,     CAMPO_Y+CAMPO_H);
         glEnd();
     }
     glColor3f(1,1,1); glLineWidth(2.f);
     glBegin(GL_LINE_LOOP);
     glVertex2f(CAMPO_X,CAMPO_Y); glVertex2f(CAMPO_X+CAMPO_W,CAMPO_Y);
     glVertex2f(CAMPO_X+CAMPO_W,CAMPO_Y+CAMPO_H); glVertex2f(CAMPO_X,CAMPO_Y+CAMPO_H);
     glEnd();
     glBegin(GL_LINES);
     glVertex2f(CAMPO_X+CAMPO_W*0.5f,CAMPO_Y);
     glVertex2f(CAMPO_X+CAMPO_W*0.5f,CAMPO_Y+CAMPO_H);
     glEnd();
     glBegin(GL_LINE_LOOP);
     float cx=CAMPO_X+CAMPO_W*0.5f, cy=CAMPO_Y+CAMPO_H*0.5f, r=60;
     for (i=0;i<64;i++) {
         float a=i*2*(float)M_PI/64;
         glVertex2f(cx+cosf(a)*r, cy+sinf(a)*r);
     }
     glEnd();
     glPointSize(6); glBegin(GL_POINTS); glVertex2f(cx,cy); glEnd();
 
     float cy_gol_min = CAMPO_Y+CAMPO_H*0.5f-GOL_H*0.5f;
     float ah=120.f, aw=80.f, pa=20.f;
     glBegin(GL_LINE_LOOP);
     glVertex2f(CAMPO_X,CAMPO_Y+CAMPO_H*0.5f-ah*0.5f);
     glVertex2f(CAMPO_X+aw,CAMPO_Y+CAMPO_H*0.5f-ah*0.5f);
     glVertex2f(CAMPO_X+aw,CAMPO_Y+CAMPO_H*0.5f+ah*0.5f);
     glVertex2f(CAMPO_X,CAMPO_Y+CAMPO_H*0.5f+ah*0.5f);
     glEnd();
     glBegin(GL_LINE_LOOP);
     glVertex2f(CAMPO_X,CAMPO_Y+CAMPO_H*0.5f-GOL_H*0.5f-pa);
     glVertex2f(CAMPO_X+pa*2,CAMPO_Y+CAMPO_H*0.5f-GOL_H*0.5f-pa);
     glVertex2f(CAMPO_X+pa*2,CAMPO_Y+CAMPO_H*0.5f+GOL_H*0.5f+pa);
     glVertex2f(CAMPO_X,CAMPO_Y+CAMPO_H*0.5f+GOL_H*0.5f+pa);
     glEnd();
     glBegin(GL_LINE_LOOP);
     glVertex2f(CAMPO_X+CAMPO_W,CAMPO_Y+CAMPO_H*0.5f-ah*0.5f);
     glVertex2f(CAMPO_X+CAMPO_W-aw,CAMPO_Y+CAMPO_H*0.5f-ah*0.5f);
     glVertex2f(CAMPO_X+CAMPO_W-aw,CAMPO_Y+CAMPO_H*0.5f+ah*0.5f);
     glVertex2f(CAMPO_X+CAMPO_W,CAMPO_Y+CAMPO_H*0.5f+ah*0.5f);
     glEnd();
     glBegin(GL_LINE_LOOP);
     glVertex2f(CAMPO_X+CAMPO_W,CAMPO_Y+CAMPO_H*0.5f-GOL_H*0.5f-pa);
     glVertex2f(CAMPO_X+CAMPO_W-pa*2,CAMPO_Y+CAMPO_H*0.5f-GOL_H*0.5f-pa);
     glVertex2f(CAMPO_X+CAMPO_W-pa*2,CAMPO_Y+CAMPO_H*0.5f+GOL_H*0.5f+pa);
     glVertex2f(CAMPO_X+CAMPO_W,CAMPO_Y+CAMPO_H*0.5f+GOL_H*0.5f+pa);
     glEnd();
     {
         float gol_y0 = cy_gol_min;
         float gol_y1 = cy_gol_min + GOL_H;
         float gx_l0 = CAMPO_X, gx_l1 = CAMPO_X - GOL_W;
         float gx_r0 = CAMPO_X + CAMPO_W, gx_r1 = CAMPO_X + CAMPO_W + GOL_W;
         int n;
 
         /* Fundo leve da rede */
         glColor4f(0.95f, 0.95f, 0.95f, 0.10f);
         glBegin(GL_QUADS);
         glVertex2f(gx_l1, gol_y0); glVertex2f(gx_l0, gol_y0);
         glVertex2f(gx_l0, gol_y1); glVertex2f(gx_l1, gol_y1);
         glEnd();
         glBegin(GL_QUADS);
         glVertex2f(gx_r0, gol_y0); glVertex2f(gx_r1, gol_y0);
         glVertex2f(gx_r1, gol_y1); glVertex2f(gx_r0, gol_y1);
         glEnd();
 
         /* Trave e contorno */
         glColor3f(0.95f, 0.95f, 0.95f);
         glLineWidth(3.f);
         glBegin(GL_LINE_LOOP);
         glVertex2f(gx_l0, gol_y0); glVertex2f(gx_l1, gol_y0);
         glVertex2f(gx_l1, gol_y1); glVertex2f(gx_l0, gol_y1);
         glEnd();
         glBegin(GL_LINE_LOOP);
         glVertex2f(gx_r0, gol_y0); glVertex2f(gx_r1, gol_y0);
         glVertex2f(gx_r1, gol_y1); glVertex2f(gx_r0, gol_y1);
         glEnd();
 
         /* Malha da rede */
         glColor4f(0.92f, 0.92f, 0.92f, 0.35f);
         glLineWidth(1.f);
         glBegin(GL_LINES);
         for (n = 1; n < 4; n++) {
             float tx = (float)n / 4.f;
             float x_l = gx_l0 + (gx_l1 - gx_l0) * tx;
             float x_r = gx_r0 + (gx_r1 - gx_r0) * tx;
             glVertex2f(x_l, gol_y0); glVertex2f(x_l, gol_y1);
             glVertex2f(x_r, gol_y0); glVertex2f(x_r, gol_y1);
         }
         for (n = 1; n < 5; n++) {
             float ty = (float)n / 5.f;
             float y = gol_y0 + (gol_y1 - gol_y0) * ty;
             glVertex2f(gx_l0, y); glVertex2f(gx_l1, y);
             glVertex2f(gx_r0, y); glVertex2f(gx_r1, y);
         }
         glEnd();
     }
     glColor3f(1,1,1); glPointSize(5);
     glBegin(GL_POINTS);
     glVertex2f(CAMPO_X+60,CAMPO_Y+CAMPO_H*0.5f);
     glVertex2f(CAMPO_X+CAMPO_W-60,CAMPO_Y+CAMPO_H*0.5f);
     glEnd();
     glLineWidth(1.f);
 }
 
 /* ─────────────────────────────────────────
   silhueta de torcedor com braços animados (para a arquibancada)
 ───────────────────────────────────────── */
 static void draw_torcedor(float xx, float yy, float escala,
                           float cr, float cg, float cb,   /* cor da camisa */
                           float sr, float sg, float sb,   /* cor da pele */
                           float brace,                    /* ângulo dos braços (animação) */
                           int lado)                       /* -1 esq, +1 dir (orientação) */
 {
     float h = escala;
     (void)lado;
 
     /* Corpo (camisa) */
     glColor3f(cr, cg, cb);
     glBegin(GL_QUADS);
     glVertex2f(xx - h*0.55f, yy);
     glVertex2f(xx + h*0.55f, yy);
     glVertex2f(xx + h*0.55f, yy + h*1.0f);
     glVertex2f(xx - h*0.55f, yy + h*1.0f);
     glEnd();
 
     /* Cabeça */
     glColor3f(sr, sg, sb);
     {
         int s;
         float cy2 = yy + h*1.35f, rad = h*0.38f;
         glBegin(GL_TRIANGLE_FAN);
        glVertex2f(xx, cy2);
         for (s = 0; s <= 10; s++) {
             float a = s * 2.f * (float)M_PI / 10.f;
             glVertex2f(xx + cosf(a)*rad, cy2 + sinf(a)*rad);
         }
         glEnd();
     }
 
     /* Braços levantados / animados */
     glColor3f(cr * 0.85f, cg * 0.85f, cb * 0.85f);
     glLineWidth(2.f);
     {
         /* Braço esquerdo */
         float bx0 = xx - h*0.55f;
         float by0 = yy + h*0.8f;
         float bx1 = bx0 - cosf(brace)*h*0.9f;
         float by1 = by0 + sinf(brace)*h*0.9f;
         glBegin(GL_LINES);
         glVertex2f(bx0, by0);
         glVertex2f(bx1, by1);
         glEnd();
 
         /* Braço direito */
         bx0 = xx + h*0.55f;
         bx1 = bx0 + cosf(brace)*h*0.9f;
         by1 = by0 + sinf(brace)*h*0.9f;
         glBegin(GL_LINES);
         glVertex2f(bx0, by0);
         glVertex2f(bx1, by1);
         glEnd();
     }
     glLineWidth(1.f);
 }
 
 /* ─────────────────────────────────────────
    HELPER: desenha uma bandeira animada
 ───────────────────────────────────────── */
 static void draw_bandeira(float fx, float fy_base, float fy_topo,
                           float cr, float cg, float cb,
                           float cr2, float cg2, float cb2,
                           float fase)
 {
     float onda1 = sinf(frame * 0.13f + fase) * 4.f;
     float onda2 = sinf(frame * 0.13f + fase + 0.6f) * 3.f;
 
     /* Haste */
     glColor3f(0.80f, 0.80f, 0.82f);
     glLineWidth(1.8f);
     glBegin(GL_LINES);
     glVertex2f(fx, fy_base);
     glVertex2f(fx, fy_topo);
     glEnd();
     glLineWidth(1.f);
 
     /* Bandeira (retângulo deformado) */
     float fw = 16.f, fh = 10.f;
     glBegin(GL_QUADS);
     /* Metade superior da bandeira */
     glColor3f(cr, cg, cb);
     glVertex2f(fx,        fy_topo);
     glVertex2f(fx + fw + onda1, fy_topo + onda2);
     glColor3f(cr2, cg2, cb2);
     glVertex2f(fx + fw + onda2, fy_topo - fh*0.5f + onda1*0.5f);
     glVertex2f(fx,        fy_topo - fh*0.5f);
     glEnd();
 
     glBegin(GL_QUADS);
     glColor3f(cr2, cg2, cb2);
     glVertex2f(fx,        fy_topo - fh*0.5f);
     glVertex2f(fx + fw + onda2, fy_topo - fh*0.5f + onda1*0.5f);
     glColor3f(cr, cg, cb);
     glVertex2f(fx + fw + onda1, fy_topo - fh + onda2*0.5f);
     glVertex2f(fx,        fy_topo - fh);
     glEnd();
 }
 
 static void draw_arquibancada_e_torcida(void) {
     float mx  = 34.f;
     float my  = 48.f;
     float sx0 = CAMPO_X - mx;
     float sx1 = CAMPO_X + CAMPO_W + mx;
     float sy0 = CAMPO_Y - my;
     float sy1 = CAMPO_Y + CAMPO_H + my;
 
     /* Número de fileiras e largura de cada degrau */
     const int FILEIRAS = 6;
     const float DEGRAU_H = 14.f;   /* altura de cada fileira */
     const float DEGRAU_EXT = 9.f;  /* recuo lateral por fileira */
 
     /* ── FUNDO DO ESTÁDIO ───────────────── */
     float total_prof = FILEIRAS * (DEGRAU_H + 1.f) + 6.f;
     glColor3f(0.12f, 0.12f, 0.14f);
     glBegin(GL_QUADS);
     glVertex2f(sx0 - total_prof - 4, sy0 - total_prof - 4);
     glVertex2f(sx1 + total_prof + 4, sy0 - total_prof - 4);
     glVertex2f(sx1 + total_prof + 4, sy1 + total_prof + 4);
     glVertex2f(sx0 - total_prof - 4, sy1 + total_prof + 4);
     glEnd();
 
     /* ── DEGRAUS (concreto, de dentro para fora) ── */
     {
         int fi;
         for (fi = 0; fi < FILEIRAS; fi++) {
             float off_i  = fi * (DEGRAU_H + 1.f);
             float off_o  = off_i + DEGRAU_H;
             float ext_i  = fi * DEGRAU_EXT;
             float ext_o  = (fi + 1) * DEGRAU_EXT;
             float bri    = 0.28f - fi * 0.03f;
 
             /* Face do degrau (painel frontal) */
             glColor3f(bri * 0.7f, bri * 0.7f, bri * 0.75f);
 
             /* Topo — faixa acima */
             glBegin(GL_QUADS);
             glVertex2f(sx0 - ext_i, sy1 + off_i);
             glVertex2f(sx1 + ext_i, sy1 + off_i);
             glVertex2f(sx1 + ext_o, sy1 + off_o);
             glVertex2f(sx0 - ext_o, sy1 + off_o);
             glEnd();
 
             /* Base — faixa abaixo */
             glBegin(GL_QUADS);
             glVertex2f(sx0 - ext_o, sy0 - off_o);
             glVertex2f(sx1 + ext_o, sy0 - off_o);
             glVertex2f(sx1 + ext_i, sy0 - off_i);
             glVertex2f(sx0 - ext_i, sy0 - off_i);
             glEnd();
 
             /* Esquerda */
             glBegin(GL_QUADS);
             glVertex2f(sx0 - ext_o, sy0 - off_o);
             glVertex2f(sx0 - ext_i, sy0 - off_i);
             glVertex2f(sx0 - ext_i, sy1 + off_i);
             glVertex2f(sx0 - ext_o, sy1 + off_o);
             glEnd();
 
             /* Direita */
             glBegin(GL_QUADS);
             glVertex2f(sx1 + ext_i, sy0 - off_i);
             glVertex2f(sx1 + ext_o, sy0 - off_o);
             glVertex2f(sx1 + ext_o, sy1 + off_o);
             glVertex2f(sx1 + ext_i, sy1 + off_i);
             glEnd();
 
             /* Linha de borda do degrau */
             glColor3f(bri * 1.4f, bri * 1.4f, bri * 1.5f);
             glLineWidth(1.0f);
             glBegin(GL_LINE_LOOP);
             glVertex2f(sx0 - ext_i, sy1 + off_i);
             glVertex2f(sx1 + ext_i, sy1 + off_i);
             glVertex2f(sx1 + ext_o, sy1 + off_o);
             glVertex2f(sx0 - ext_o, sy1 + off_o);
             glEnd();
             glBegin(GL_LINE_LOOP);
             glVertex2f(sx0 - ext_o, sy0 - off_o);
             glVertex2f(sx1 + ext_o, sy0 - off_o);
             glVertex2f(sx1 + ext_i, sy0 - off_i);
             glVertex2f(sx0 - ext_i, sy0 - off_i);
             glEnd();
         }
     }
 
     /* ── TORCEDORES ────────────────────── */
     {
         int fi, i;
         /* Cores de camisa das torcidas (mistura dos dois times + neutros) */
         CorTime *ct0 = &paleta[cor_selecionada[0]];
         CorTime *ct1 = &paleta[cor_selecionada[1]];
 
         /* Paleta de cores de camisa: time0, time1, neutros */
         float shirt_r[6] = {ct0->r, ct1->r, 0.9f, 0.9f, 0.2f, ct0->r*0.8f+0.15f};
         float shirt_g[6] = {ct0->g, ct1->g, 0.9f, 0.3f, 0.2f, ct1->g*0.8f+0.15f};
         float shirt_b[6] = {ct0->b, ct1->b, 0.9f, 0.3f, 0.8f, ct0->b*0.5f+ct1->b*0.5f};
 
         /* Tons de pele variados */
         float pele_r[4] = {0.95f, 0.82f, 0.72f, 0.55f};
         float pele_g[4] = {0.80f, 0.65f, 0.52f, 0.38f};
         float pele_b[4] = {0.70f, 0.55f, 0.42f, 0.28f};
 
         /* Fileiras superiores e inferiores */
         for (fi = 0; fi < FILEIRAS; fi++) {
             float off_i = fi * (DEGRAU_H + 1.f);
             float off_c = off_i + DEGRAU_H * 0.45f; /* centro vertical da fileira */
             float ext_i = fi * DEGRAU_EXT;
 
             /* Escala diminui conforme fileira vai para fora (perspectiva) */
             float escala = 5.5f - fi * 0.55f;
             float espac  = escala * 2.6f;
 
             /* ─── Faixa superior ─── */
             {
                 float faixa_y = sy1 + off_c + escala * 0.5f;
                 float x_ini   = sx0 - ext_i + espac;
                 float x_fim   = sx1 + ext_i - espac;
                 int n = (int)((x_fim - x_ini) / espac);
                 if (n < 1) n = 1;
                 for (i = 0; i < n; i++) {
                     float xx  = x_ini + i * (x_fim - x_ini) / (float)(n - 1 + (n==1));
                     int   sc  = ((i * 3 + fi * 7) ^ (i + fi * 2)) % 6;
                     int   pc  = (i + fi) % 4;
                     /* Animação: onda de bracejo */
                     float fase_b = frame * 0.07f + i * 0.35f + fi * 0.6f;
                     float brace  = (float)M_PI * 0.45f + sinf(fase_b) * 0.4f;
                     float escura = 1.f - fi * 0.06f;
                     draw_torcedor(xx, faixa_y, escala,
                                   shirt_r[sc]*escura, shirt_g[sc]*escura, shirt_b[sc]*escura,
                                   pele_r[pc]*escura,  pele_g[pc]*escura,  pele_b[pc]*escura,
                                   brace, (i%2 ? 1:-1));
                 }
             }
 
             /* ─── Faixa inferior ─── */
             {
                 float faixa_y = sy0 - off_c - escala * 1.8f;
                 float x_ini   = sx0 - ext_i + espac;
                 float x_fim   = sx1 + ext_i - espac;
                 int n = (int)((x_fim - x_ini) / espac);
                 if (n < 1) n = 1;
                 for (i = 0; i < n; i++) {
                     float xx  = x_ini + i * (x_fim - x_ini) / (float)(n - 1 + (n==1));
                     int   sc  = ((i * 5 + fi * 3 + 1) ^ (i*2 + fi)) % 6;
                     int   pc  = (i * 2 + fi + 1) % 4;
                     float fase_b = frame * 0.07f + i * 0.4f + fi * 0.5f + 1.3f;
                     float brace  = (float)M_PI * 0.45f + sinf(fase_b) * 0.4f;
                     float escura = 1.f - fi * 0.06f;
                     draw_torcedor(xx, faixa_y, escala,
                                   shirt_r[sc]*escura, shirt_g[sc]*escura, shirt_b[sc]*escura,
                                   pele_r[pc]*escura,  pele_g[pc]*escura,  pele_b[pc]*escura,
                                   brace, (i%2 ? -1:1));
                 }
             }
 
             /* ─── Faixa lateral esquerda ─── */
             {
                 float faixa_x = sx0 - ext_i - escala * 0.5f;
                 float y_ini   = sy0 + espac;
                 float y_fim   = sy1 - espac;
                 int n = (int)((y_fim - y_ini) / espac);
                 if (n < 1) n = 1;
                 for (i = 0; i < n; i++) {
                     float yy  = y_ini + i * (y_fim - y_ini) / (float)(n - 1 + (n==1));
                     int   sc  = ((i * 2 + fi * 4 + 2) ^ (i + fi * 3)) % 6;
                     int   pc  = (i + fi * 2 + 2) % 4;
                     float fase_b = frame * 0.07f + i * 0.45f + fi * 0.7f + 2.1f;
                     float brace  = (float)M_PI * 0.45f + sinf(fase_b) * 0.4f;
                     float escura = 1.f - fi * 0.06f;
                     draw_torcedor(faixa_x, yy, escala,
                                   shirt_r[sc]*escura, shirt_g[sc]*escura, shirt_b[sc]*escura,
                                   pele_r[pc]*escura,  pele_g[pc]*escura,  pele_b[pc]*escura,
                                   brace, 1);
                 }
             }
 
             /* ─── Faixa lateral direita ─── */
             {
                 float faixa_x = sx1 + ext_i + escala * 0.5f;
                 float y_ini   = sy0 + espac;
                 float y_fim   = sy1 - espac;
                 int n = (int)((y_fim - y_ini) / espac);
                 if (n < 1) n = 1;
                 for (i = 0; i < n; i++) {
                     float yy  = y_ini + i * (y_fim - y_ini) / (float)(n - 1 + (n==1));
                     int   sc  = ((i * 4 + fi * 2 + 3) ^ (i * 3 + fi)) % 6;
                     int   pc  = (i * 3 + fi + 3) % 4;
                     float fase_b = frame * 0.07f + i * 0.5f + fi * 0.55f + 3.7f;
                     float brace  = (float)M_PI * 0.45f + sinf(fase_b) * 0.4f;
                     float escura = 1.f - fi * 0.06f;
                     draw_torcedor(faixa_x, yy, escala,
                                   shirt_r[sc]*escura, shirt_g[sc]*escura, shirt_b[sc]*escura,
                                   pele_r[pc]*escura,  pele_g[pc]*escura,  pele_b[pc]*escura,
                                   brace, -1);
                 }
             }
         }
     }
 
     /* ── BANDEIRAS (faixa superior) ── */
     {
         int k;
         CorTime *ct0 = &paleta[cor_selecionada[0]];
         CorTime *ct1 = &paleta[cor_selecionada[1]];
         int n_band = 10;
         for (k = 0; k < n_band; k++) {
             float fx     = sx0 + 30.f + k * ((sx1 - sx0 - 60.f) / (n_band - 1));
             float fy_b   = sy1 + 4.f;
             float fy_t   = sy1 + 26.f;
             float fase   = k * 0.9f;
 
             /* Alterna entre cores dos times */
             if (k % 3 == 0) {
                 draw_bandeira(fx, fy_b, fy_t,
                               ct0->r, ct0->g, ct0->b,
                               0.95f, 0.95f, 0.95f, fase);
             } else if (k % 3 == 1) {
                 draw_bandeira(fx, fy_b, fy_t,
                               ct1->r, ct1->g, ct1->b,
                               0.95f, 0.95f, 0.95f, fase);
             } else {
                 draw_bandeira(fx, fy_b, fy_t,
                               ct0->r, ct0->g, ct0->b,
                               ct1->r, ct1->g, ct1->b, fase);
             }
         }
     }
 
     /* ── MURETA (bordão do campo) ── */
     glColor3f(0.82f, 0.82f, 0.85f);
     glLineWidth(3.0f);
     glBegin(GL_LINE_LOOP);
     glVertex2f(sx0, sy0);
     glVertex2f(sx1, sy0);
     glVertex2f(sx1, sy1);
     glVertex2f(sx0, sy1);
     glEnd();
 
     /* Faixa colorida na mureta (propaganda) */
     glColor3f(0.15f, 0.38f, 0.72f);
     glBegin(GL_QUADS);
     glVertex2f(sx0, sy0 - 5.f);
     glVertex2f(sx1, sy0 - 5.f);
     glVertex2f(sx1, sy0);
     glVertex2f(sx0, sy0);
     glEnd();
     glColor3f(0.15f, 0.38f, 0.72f);
     glBegin(GL_QUADS);
     glVertex2f(sx0, sy1);
     glVertex2f(sx1, sy1);
     glVertex2f(sx1, sy1 + 5.f);
     glVertex2f(sx0, sy1 + 5.f);
     glEnd();
     glLineWidth(1.f);
 }
 
 /* ─────────────────────────────────────────
    INDICADOR DE PASSE — linha do passador ao receptor
 ───────────────────────────────────────── */
 static void draw_passe_visual(int t) {
     if (passe_visual_timer[t] <= 0) return;
     /* Receptor foi transferido; desenhamos do passador (posição bola) ao destino */
     float alpha = (float)passe_visual_timer[t] / 35.f;
     CorTime *ct = &paleta[cor_selecionada[t]];
 
     /* Linha tracejada bola → destino */
     glColor4f(ct->r, ct->g, ct->b, alpha * 0.8f);
     glLineWidth(2.5f);
     glLineStipple(4, 0xAAAA);
     glEnable(GL_LINE_STIPPLE);
     glBegin(GL_LINES);
     glVertex2f(bola.x, bola.y);
     glVertex2f(passe_visual_x[t], passe_visual_y[t]);
     glEnd();
     glDisable(GL_LINE_STIPPLE);
 
     /* Círculo no receptor */
     glColor4f(ct->r, ct->g, ct->b, alpha * 0.5f);
     int i;
     float rx = passe_visual_x[t], ry = passe_visual_y[t];
     glBegin(GL_LINE_LOOP);
     for (i = 0; i < 24; i++) {
         float a = i * 2.f * (float)M_PI / 24.f;
         glVertex2f(rx + cosf(a)*(RAIO_JOGADOR+7), ry + sinf(a)*(RAIO_JOGADOR+7));
     }
     glEnd();
     glLineWidth(1.f);
 
     passe_visual_timer[t]--;
 }
 
 /* ─────────────────────────────────────────
    DESENHO DO JOGADOR
 ───────────────────────────────────────── */
 static void draw_jogador(Jogador *p, float cr, float cg, float cb, int is_humano) {
     float x=p->x, y=p->y;
   float angd=atan2f(p->vy,p->vx);
     int i;
   float dirx = cosf(angd), diry = sinf(angd);
   float side_x = -diry, side_y = dirx;
   float body_w = RAIO_JOGADOR * 0.76f;
   float body_h = RAIO_JOGADOR * 1.10f;
   float short_h = RAIO_JOGADOR * 0.40f;
   float head_r = RAIO_JOGADOR * 0.39f;
   float leg_len = RAIO_JOGADOR * 1.00f;
   float arm_len = RAIO_JOGADOR * 0.82f;
   float run = sinf(p->ang_perna * (float)M_PI / 180.f);
   float skin_r = 0.95f, skin_g = 0.80f, skin_b = 0.65f;
   float shadow_rx = RAIO_JOGADOR * 1.00f;
   float shadow_ry = RAIO_JOGADOR * 0.50f;
   float chest_x = x + dirx * (body_h * 0.12f);
   float chest_y = y + diry * (body_h * 0.12f);
   float neck_x = x + dirx * (body_h * 1.02f);
   float neck_y = y + diry * (body_h * 1.02f);
   float hx = x + dirx * (body_h + head_r * 0.38f);
   float hy = y + diry * (body_h + head_r * 0.38f);
   float light_x = -0.55f, light_y = 0.75f;

    glColor4f(0.f,0.f,0.f,0.28f);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(x+2.5f,y-2.5f);
    for(i=0;i<=24;i++){
        float a=i*2*(float)M_PI/24;
        glVertex2f(x+2.5f+cosf(a)*shadow_rx, y-2.5f+sinf(a)*shadow_ry);
    }
    glEnd();

   glLineWidth(4.2f);
   glColor3f(0.10f,0.09f,0.12f);
    glBegin(GL_LINES);
    glVertex2f(x - side_x*body_w*0.28f, y - side_y*body_w*0.28f);
    glVertex2f(x - side_x*body_w*0.16f - dirx*leg_len*(0.42f + run*0.22f),
               y - side_y*body_w*0.16f - diry*leg_len*(0.42f + run*0.22f));
    glVertex2f(x + side_x*body_w*0.28f, y + side_y*body_w*0.28f);
    glVertex2f(x + side_x*body_w*0.16f - dirx*leg_len*(0.42f - run*0.22f),
               y + side_y*body_w*0.16f - diry*leg_len*(0.42f - run*0.22f));
    glEnd();

   glLineWidth(4.0f);
   glColor3f(0.92f,0.92f,0.95f);
   glBegin(GL_LINES);
   glVertex2f(x - side_x*body_w*0.24f - dirx*leg_len*(0.45f + run*0.20f),
              y - side_y*body_w*0.24f - diry*leg_len*(0.45f + run*0.20f));
   glVertex2f(x - side_x*body_w*0.22f - dirx*leg_len*(0.62f + run*0.16f),
              y - side_y*body_w*0.22f - diry*leg_len*(0.62f + run*0.16f));
   glVertex2f(x + side_x*body_w*0.24f - dirx*leg_len*(0.45f - run*0.20f),
              y + side_y*body_w*0.24f - diry*leg_len*(0.45f - run*0.20f));
   glVertex2f(x + side_x*body_w*0.22f - dirx*leg_len*(0.62f - run*0.16f),
              y + side_y*body_w*0.22f - diry*leg_len*(0.62f - run*0.16f));
   glEnd();

   glLineWidth(3.0f);
   glColor3f(skin_r*0.92f, skin_g*0.88f, skin_b*0.84f);
    glBegin(GL_LINES);
    glVertex2f(x - side_x*body_w*0.78f + dirx*RAIO_JOGADOR*0.15f,
               y - side_y*body_w*0.78f + diry*RAIO_JOGADOR*0.15f);
    glVertex2f(x - side_x*arm_len, y - side_y*arm_len + run*2.0f);
    glVertex2f(x + side_x*body_w*0.78f + dirx*RAIO_JOGADOR*0.15f,
               y + side_y*body_w*0.78f + diry*RAIO_JOGADOR*0.15f);
    glVertex2f(x + side_x*arm_len, y + side_y*arm_len - run*2.0f);
    glEnd();

   glColor3f(cr*0.45f,cg*0.45f,cb*0.45f);
    glBegin(GL_TRIANGLE_FAN);
   glVertex2f(chest_x - dirx*RAIO_JOGADOR*0.18f, chest_y - diry*RAIO_JOGADOR*0.18f);
    for(i=0;i<=28;i++){
        float a=i*2*(float)M_PI/28.f;
        float ex = cosf(a)*body_w + dirx*body_h*sinf(a)*0.34f;
        float ey = sinf(a)*body_h + diry*body_h*sinf(a)*0.34f;
        glVertex2f(chest_x + ex, chest_y + ey);
    }
    glEnd();

    glColor3f(cr,cg,cb);
    glBegin(GL_TRIANGLE_FAN);
   glVertex2f(chest_x - dirx*RAIO_JOGADOR*0.24f + side_x*body_w*0.12f*light_x,
              chest_y - diry*RAIO_JOGADOR*0.24f + body_h*0.10f*light_y);
    for(i=0;i<=28;i++){
        float a=i*2*(float)M_PI/28.f;
        float ex = cosf(a)*body_w*0.90f + dirx*body_h*sinf(a)*0.18f;
        float ey = sinf(a)*body_h*0.92f + diry*body_h*sinf(a)*0.18f;
        glVertex2f(chest_x + ex, chest_y + ey);
    }
    glEnd();

   glColor4f(1.f,1.f,1.f,0.16f);
   glBegin(GL_TRIANGLE_FAN);
   glVertex2f(chest_x + side_x*body_w*0.08f + dirx*body_h*0.10f,
              chest_y + side_y*body_w*0.08f + diry*body_h*0.10f);
   for(i=0;i<=22;i++){
       float a = -0.2f + i*(float)M_PI/22.f;
       glVertex2f(chest_x + side_x*cosf(a)*body_w*0.48f + dirx*sinf(a)*body_h*0.42f,
                  chest_y + side_y*cosf(a)*body_w*0.48f + diry*sinf(a)*body_h*0.42f);
   }
    glEnd();

    glColor3f(cr*0.32f,cg*0.32f,cb*0.32f);
    glBegin(GL_QUADS);
    glVertex2f(x - side_x*body_w*0.78f - dirx*short_h*0.15f, y - side_y*body_w*0.78f - diry*short_h*0.15f);
    glVertex2f(x + side_x*body_w*0.78f - dirx*short_h*0.15f, y + side_y*body_w*0.78f - diry*short_h*0.15f);
    glVertex2f(x + side_x*body_w*0.62f - dirx*(short_h + RAIO_JOGADOR*0.12f), y + side_y*body_w*0.62f - diry*(short_h + RAIO_JOGADOR*0.12f));
    glVertex2f(x - side_x*body_w*0.62f - dirx*(short_h + RAIO_JOGADOR*0.12f), y - side_y*body_w*0.62f - diry*(short_h + RAIO_JOGADOR*0.12f));
    glEnd();

   glColor3f(0.20f,0.20f,0.22f);
   glBegin(GL_QUADS);
   glVertex2f(neck_x - side_x*body_w*0.22f - dirx*3.0f, neck_y - side_y*body_w*0.22f - diry*3.0f);
   glVertex2f(neck_x + side_x*body_w*0.22f - dirx*3.0f, neck_y + side_y*body_w*0.22f - diry*3.0f);
   glVertex2f(neck_x + side_x*body_w*0.14f + dirx*2.0f, neck_y + side_y*body_w*0.14f + diry*2.0f);
   glVertex2f(neck_x - side_x*body_w*0.14f + dirx*2.0f, neck_y - side_y*body_w*0.14f + diry*2.0f);
   glEnd();

   glColor3f(skin_r*0.70f, skin_g*0.66f, skin_b*0.62f);
   glBegin(GL_TRIANGLE_FAN); glVertex2f(hx + dirx*1.1f, hy + diry*1.1f);
   for(i=0;i<=24;i++){
       float a=i*2*(float)M_PI/24.f;
       glVertex2f(hx+cosf(a)*head_r, hy+sinf(a)*head_r*1.05f);
   }
   glEnd();

   glColor3f(skin_r, skin_g, skin_b);
   glBegin(GL_TRIANGLE_FAN); glVertex2f(hx - dirx*0.55f + side_x*head_r*0.08f, hy - diry*0.55f + side_y*head_r*0.08f);
   for(i=0;i<=24;i++){
       float a=i*2*(float)M_PI/24.f;
       glVertex2f(hx+cosf(a)*head_r*0.92f, hy+sinf(a)*head_r*0.97f);
   }
   glEnd();

   glColor4f(1.f,1.f,1.f,0.18f);
   glBegin(GL_TRIANGLE_FAN); glVertex2f(hx - side_x*head_r*0.18f + dirx*head_r*0.04f,
                                        hy - side_y*head_r*0.18f + diry*head_r*0.04f);
   for(i=0;i<=16;i++){
       float a = 0.35f + i*(float)M_PI/16.f;
       glVertex2f(hx + side_x*cosf(a)*head_r*0.34f + dirx*sinf(a)*head_r*0.28f,
                  hy + side_y*cosf(a)*head_r*0.34f + diry*sinf(a)*head_r*0.28f);
   }
   glEnd();

   glColor3f(0.12f,0.09f,0.06f);
   glBegin(GL_TRIANGLE_FAN); glVertex2f(hx + dirx*head_r*0.12f, hy + diry*head_r*0.12f);
   for(i=0;i<=18;i++){
       float a=(float)M_PI + i*(float)M_PI/18.f;
       glVertex2f(hx+cosf(a)*head_r*0.98f, hy+sinf(a)*head_r*0.76f + diry*head_r*0.08f);
   }
   glEnd();

   glLineWidth(4.8f);
   glColor3f(0.08f,0.08f,0.10f);
   glBegin(GL_LINES);
   glVertex2f(x - side_x*body_w*0.18f - dirx*leg_len*(0.66f + run*0.10f),
              y - side_y*body_w*0.18f - diry*leg_len*(0.66f + run*0.10f));
   glVertex2f(x - side_x*body_w*0.42f - dirx*leg_len*(0.77f + run*0.10f),
              y - side_y*body_w*0.42f - diry*leg_len*(0.77f + run*0.10f));
   glVertex2f(x + side_x*body_w*0.18f - dirx*leg_len*(0.66f - run*0.10f),
              y + side_y*body_w*0.18f - diry*leg_len*(0.66f - run*0.10f));
   glVertex2f(x + side_x*body_w*0.42f - dirx*leg_len*(0.77f - run*0.10f),
              y + side_y*body_w*0.42f - diry*leg_len*(0.77f - run*0.10f));
   glEnd();

   glLineWidth(1.2f);
   glColor4f(0.f,0.f,0.f,0.28f);
   glBegin(GL_LINE_LOOP);
   for(i=0;i<=28;i++){
       float a=i*2*(float)M_PI/28.f;
       float ex = cosf(a)*body_w*0.92f + dirx*body_h*sinf(a)*0.18f;
       float ey = sinf(a)*body_h*0.92f + diry*body_h*sinf(a)*0.18f;
       glVertex2f(chest_x + ex, chest_y + ey);
   }
   glEnd();
     /* Barra de energia abaixo do jogador */
     {
         float bw = RAIO_JOGADOR * 2.f, bh = 2.5f;
         float bx2 = x - RAIO_JOGADOR, by2 = y - RAIO_JOGADOR - 5.f;
         glColor3f(0.2f, 0.2f, 0.2f);
         glBegin(GL_QUADS);
         glVertex2f(bx2,by2); glVertex2f(bx2+bw,by2);
         glVertex2f(bx2+bw,by2+bh); glVertex2f(bx2,by2+bh);
         glEnd();
         float ef = p->energia;
         float er = (ef < 0.5f) ? 1.f : 2.f - ef*2.f;
         float eg = (ef > 0.5f) ? 1.f : ef*2.f;
         glColor3f(er, eg, 0.f);
         glBegin(GL_QUADS);
         glVertex2f(bx2,by2); glVertex2f(bx2+bw*ef,by2);
         glVertex2f(bx2+bw*ef,by2+bh); glVertex2f(bx2,by2+bh);
         glEnd();
     }
 
     if (p->com_bola) {
         glColor3f(1.f,1.f,0.f); glLineWidth(2.f);
         glBegin(GL_LINE_LOOP);
         for(i=0;i<=20;i++){float a=i*2*(float)M_PI/20;glVertex2f(x+cosf(a)*(RAIO_JOGADOR+4),y+sinf(a)*(RAIO_JOGADOR+4));}
         glEnd();
     }
 
     if (is_humano) {
         float pulse = 0.6f + 0.4f*sinf(frame*0.22f);
         glColor4f(1.f, 1.f, 1.f, pulse);
         glLineWidth(2.f);
         float ty2 = y + RAIO_JOGADOR + 5;
         glBegin(GL_TRIANGLES);
         glVertex2f(x,    ty2+9);
         glVertex2f(x-5,  ty2);
         glVertex2f(x+5,  ty2);
         glEnd();
     }
 
     if (p->marcando >= 0 && !is_humano) {
         int adv = 1 - p->time;
         Jogador *alvo = &jogadores[adv][p->marcando];
         glColor4f(1.f, 0.4f, 0.f, 0.35f);
         glLineWidth(1.f);
         glBegin(GL_LINES);
         glVertex2f(x, y);
         glVertex2f(alvo->x, alvo->y);
         glEnd();
     }
 
     glLineWidth(1.f);
 }
 
 /* ─────────────────────────────────────────
    DESENHO DA BOLA
 ───────────────────────────────────────── */
 static void draw_bola(void) {
     float x=bola.x, y=bola.y;
     int i;
     glColor4f(0,0,0,0.25f);
     glBegin(GL_TRIANGLE_FAN); glVertex2f(x+3,y-3);
     for(i=0;i<=20;i++){float a=i*2*(float)M_PI/20;glVertex2f(x+3+cosf(a)*RAIO_BOLA,y-3+sinf(a)*RAIO_BOLA*0.5f);}
     glEnd();
     glColor3f(0.95f,0.95f,0.95f);
     glBegin(GL_TRIANGLE_FAN); glVertex2f(x,y);
     for(i=0;i<=24;i++){float a=i*2*(float)M_PI/24;glVertex2f(x+cosf(a)*RAIO_BOLA,y+sinf(a)*RAIO_BOLA);}
     glEnd();
     glColor3f(0.1f,0.1f,0.1f);
     static float hex_ang[6]={0,60,120,180,240,300};
     for(int h=0;h<6;h++){
         float ha=(hex_ang[h]+frame*2)*(float)M_PI/180.f;
         float px2=x+cosf(ha)*RAIO_BOLA*0.55f, py2=y+sinf(ha)*RAIO_BOLA*0.55f;
         glBegin(GL_TRIANGLE_FAN); glVertex2f(px2,py2);
         for(i=0;i<=6;i++){float a=i*2*(float)M_PI/6;glVertex2f(px2+cosf(a)*RAIO_BOLA*0.22f,py2+sinf(a)*RAIO_BOLA*0.22f);}
         glEnd();
     }
     glColor3f(0,0,0); glLineWidth(1.5f);
     glBegin(GL_LINE_LOOP);
     for(i=0;i<=24;i++){float a=i*2*(float)M_PI/24;glVertex2f(x+cosf(a)*RAIO_BOLA,y+sinf(a)*RAIO_BOLA);}
     glEnd();
     glLineWidth(1.f);
 }
 
enum {
    HUD_STATUS_NEUTRO = 0,
    HUD_STATUS_BOLA,
    HUD_STATUS_MARCA
};

static int texto_status_hud_time(int time, char *buf, size_t buf_sz, int *status_out) {
    int i, idx = -1;
    Jogador *p = NULL;

    if (status_out) *status_out = HUD_STATUS_NEUTRO;

    for (i = 0; i < NUM_JOGADORES; i++) {
        if (jogadores[time][i].com_bola) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        for (i = 0; i < NUM_JOGADORES; i++) {
            if (jogadores[time][i].marcando >= 0 && jogadores[time][i].marcando < NUM_JOGADORES) {
                idx = i;
                break;
            }
        }
    }

    if (idx < 0) idx = jogador_humano[time];
    if (idx < 0 || idx >= NUM_JOGADORES) idx = 0;

    p = &jogadores[time][idx];
    if (p->com_bola) {
        snprintf(buf, buf_sz, "%02d", p->numero);
        if (status_out) *status_out = HUD_STATUS_BOLA;
    } else if (p->marcando >= 0 && p->marcando < NUM_JOGADORES) {
        Jogador *alvo = &jogadores[1 - time][p->marcando];
        snprintf(buf, buf_sz, "%02d x %02d", p->numero, alvo->numero);
        if (status_out) *status_out = HUD_STATUS_MARCA;
    } else {
        snprintf(buf, buf_sz, "%02d", p->numero);
    }
    return 1;
}

 /* ─────────────────────────────────────────
    HUD
 ───────────────────────────────────────── */
 static void draw_hud(void) {
     float placar_w = 300.f, placar_h = 47.f;
     float placar_x = (LARGURA_JANELA - placar_w) * 0.5f;
     float placar_y = fmaxf(6.f, CAMPO_Y - 50.f);
     float info_x = 10.f;
     float info_y1 = ALTURA_JANELA - 18.f;
     float info_y2 = ALTURA_JANELA - 33.f;
     float banner_w = 360.f;
     float banner_x = (LARGURA_JANELA - banner_w) * 0.5f;
     float banner_y = CAMPO_Y + CAMPO_H + 20.f;
 
     glColor4f(0,0,0,0.7f);
     glBegin(GL_QUADS);
     glVertex2f(placar_x,placar_y);
     glVertex2f(placar_x+placar_w,placar_y);
     glVertex2f(placar_x+placar_w,placar_y+placar_h);
     glVertex2f(placar_x,placar_y+placar_h);
     glEnd();
 
     CorTime *c0=&paleta[cor_selecionada[0]], *c1=&paleta[cor_selecionada[1]];
     char buf[64];
    int status_a = HUD_STATUS_NEUTRO, status_b = HUD_STATUS_NEUTRO;
 
    glColor3f(c0->r,c0->g,c0->b);
    draw_text(placar_x + 14.f,placar_y + 30.f,"Time A",GLUT_BITMAP_HELVETICA_18);
     glColor3f(1,1,1);
     snprintf(buf,sizeof(buf),"%d : %d",placar[0],placar[1]);
     draw_text(placar_x + 124.f,placar_y + 30.f,buf,GLUT_BITMAP_HELVETICA_18);
     glColor3f(c1->r,c1->g,c1->b);
     draw_text(placar_x + 220.f,placar_y + 30.f,"Time B",GLUT_BITMAP_HELVETICA_18);

    if (texto_status_hud_time(0, buf, sizeof(buf), &status_a)){
        float tag_x = placar_x + 14.f, tag_y = placar_y + 5.f, tag_w = 82.f, tag_h = 15.f;
        if (status_a == HUD_STATUS_BOLA) glColor4f(0.10f,0.62f,0.18f,0.90f);
        else if (status_a == HUD_STATUS_MARCA) glColor4f(0.90f,0.48f,0.10f,0.90f);
        else glColor4f(0.38f,0.38f,0.42f,0.90f);
        glBegin(GL_QUADS);
        glVertex2f(tag_x,tag_y); glVertex2f(tag_x+tag_w,tag_y);
        glVertex2f(tag_x+tag_w,tag_y+tag_h); glVertex2f(tag_x,tag_y+tag_h);
        glEnd();
        glColor3f(1.f,1.f,1.f);
        if (status_a == HUD_STATUS_BOLA) draw_text(tag_x + 5.f, placar_y + 10.f, "BOLA", GLUT_BITMAP_HELVETICA_10);
        else if (status_a == HUD_STATUS_MARCA) draw_text(tag_x + 4.f, placar_y + 10.f, "MARCA", GLUT_BITMAP_HELVETICA_10);
        else draw_text(tag_x + 4.f, placar_y + 10.f, "ATIVO", GLUT_BITMAP_HELVETICA_10);
        draw_text(tag_x + 46.f, placar_y + 10.f, buf, GLUT_BITMAP_HELVETICA_12);
    }
    if (texto_status_hud_time(1, buf, sizeof(buf), &status_b)){
        float tag_x = placar_x + 204.f, tag_y = placar_y + 5.f, tag_w = 82.f, tag_h = 15.f;
        if (status_b == HUD_STATUS_BOLA) glColor4f(0.10f,0.62f,0.18f,0.90f);
        else if (status_b == HUD_STATUS_MARCA) glColor4f(0.90f,0.48f,0.10f,0.90f);
        else glColor4f(0.38f,0.38f,0.42f,0.90f);
        glBegin(GL_QUADS);
        glVertex2f(tag_x,tag_y); glVertex2f(tag_x+tag_w,tag_y);
        glVertex2f(tag_x+tag_w,tag_y+tag_h); glVertex2f(tag_x,tag_y+tag_h);
        glEnd();
        glColor3f(1.f,1.f,1.f);
        if (status_b == HUD_STATUS_BOLA) draw_text(tag_x + 5.f, placar_y + 10.f, "BOLA", GLUT_BITMAP_HELVETICA_10);
        else if (status_b == HUD_STATUS_MARCA) draw_text(tag_x + 4.f, placar_y + 10.f, "MARCA", GLUT_BITMAP_HELVETICA_10);
        else draw_text(tag_x + 4.f, placar_y + 10.f, "ATIVO", GLUT_BITMAP_HELVETICA_10);
        draw_text(tag_x + 46.f, placar_y + 10.f, buf, GLUT_BITMAP_HELVETICA_12);
    }
 
     /* tempo_jogo em frames (60fps). 45min jogo = 10800 frames = 3min real.
        1 minuto de jogo = 240 frames */
     int min_jogo = tempo_jogo / 240;
     int seg_jogo = (tempo_jogo % 240) * 60 / 240;
     glColor3f(0.8f,0.8f,0.8f);
     snprintf(buf,sizeof(buf),"%02d:%02d",min_jogo,seg_jogo);
     draw_text(placar_x + 125.f,placar_y + 15.f,buf,GLUT_BITMAP_HELVETICA_12);
 
     glColor3f(0.7f,0.7f,0.7f);
     if (modo_jogo == MODO_1P) {
         draw_text(info_x,info_y1,"Setas:mover | C:sprint | Espaco:chute | Z:passe | X:tackle | ESC:sair",
                   GLUT_BITMAP_HELVETICA_12);
         draw_text(info_x,info_y2,"Passe transfere controle ao receptor automaticamente",
                   GLUT_BITMAP_HELVETICA_12);
     } else {
         draw_text(info_x,info_y1,"J1:Setas+C(sprint)+Espaco(chute)+Z(passe)+X(tackle)  |  J2:WASD+R(sprint)+F(chute)+H(passe)+G(tackle)",
                   GLUT_BITMAP_HELVETICA_12);
         draw_text(info_x,info_y2,"Passe transfere o controle ao receptor | ESC:sair",
                   GLUT_BITMAP_HELVETICA_12);
     }
 
     /* Indicador de lateral / escanteio */
     if (reposicao_tipo != REPOSICAO_NENHUMA) {
         const char *tipo_str = (reposicao_tipo == LATERAL) ? "LATERAL"
                                : (reposicao_tipo == ESCANTEIO) ? "ESCANTEIO"
                                : "TIRO DE META";
         CorTime *ct = &paleta[cor_selecionada[reposicao_time]];
         float pulse = 0.6f + 0.4f * sinf(frame * 0.25f);
         glColor4f(0,0,0,0.75f);
         glBegin(GL_QUADS);
         glVertex2f(banner_x,banner_y); glVertex2f(banner_x+banner_w,banner_y);
         glVertex2f(banner_x+banner_w,banner_y+25.f); glVertex2f(banner_x,banner_y+25.f);
         glEnd();
         glColor4f(ct->r*pulse, ct->g*pulse, ct->b*pulse, 1.f);
         snprintf(buf, sizeof(buf), "%s - Time %s",
                  tipo_str, reposicao_time==0 ? "A" : "B");
         draw_text(banner_x + 100.f, banner_y + 5.f, buf, GLUT_BITMAP_HELVETICA_18);
         /* Marca a bola com um círculo pulsante */
         glColor4f(1.f, 1.f, 0.f, pulse * 0.8f);
         glLineWidth(2.5f);
         int i2;
         glBegin(GL_LINE_LOOP);
         for (i2 = 0; i2 < 24; i2++) {
             float a = i2 * 2.f * (float)M_PI / 24.f;
             glVertex2f(reposicao_bola_x + cosf(a)*(RAIO_BOLA+8),
                        reposicao_bola_y + sinf(a)*(RAIO_BOLA+8));
         }
         glEnd();
         glLineWidth(1.f);
     }
 }
 
/* ─────────────────────────────────────────
   INTRO (video intro.mp4)
   Renderizamos via frames extraídos em PPM.
   ───────────────────────────────────────── */
static int intro_path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void intro_ensure_frames_extracted(void) {
    if (intro_ready || intro_extract_failed) return;

    const char *frames_dir = "video/intro_frames";
    struct stat st;
    if (stat(frames_dir, &st) != 0) {
        (void)mkdir(frames_dir, 0755);
    }

    char first_path[512];
    snprintf(first_path, sizeof(first_path), "%s/frame_%05d.ppm", frames_dir, 0);

    if (!intro_path_exists(first_path)) {
        /* Extrai frames em baixa resolução para reduzir upload na textura */
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "ffmpeg -y -v error -nostdin -i \"video/intro.mp4\" "
                 "-vf \"fps=15,scale=960:600\" -start_number 0 "
                 "\"%s/frame_%%05d.ppm\"",
                 frames_dir);
        (void)system(cmd);
    }

    DIR *d = opendir(frames_dir);
    if (!d) {
        intro_extract_failed = 1;
        return;
    }

    int max_idx = -1;
    while (1) {
        struct dirent *ent = readdir(d);
        if (!ent) break;
        int idx;
        if (sscanf(ent->d_name, "frame_%05d.ppm", &idx) == 1) {
            if (idx > max_idx) max_idx = idx;
        }
    }
    closedir(d);

    if (max_idx < 0) {
        intro_extract_failed = 1;
        return;
    }

    intro_frame_count = max_idx + 1;
    if (intro_frame_count <= 0) {
        intro_extract_failed = 1;
        return;
    }

    intro_ready = 1;
}

static int intro_load_ppm_rgb(const char *path, unsigned char **out_pixels, int *out_w, int *out_h) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;

    char magic[3] = {0};
    if (fscanf(fp, "%2s", magic) != 1) {
        fclose(fp);
        return 0;
    }
    if (strcmp(magic, "P6") != 0) {
        fclose(fp);
        return 0;
    }

    int w = 0, h = 0, maxv = 0;
    if (fscanf(fp, "%d %d", &w, &h) != 2) {
        fclose(fp);
        return 0;
    }
    if (fscanf(fp, "%d", &maxv) != 1) {
        fclose(fp);
        return 0;
    }
    if (w <= 0 || h <= 0 || maxv != 255) {
        fclose(fp);
        return 0;
    }

    /* Consome o whitespace após o maxval */
    (void)fgetc(fp);

    size_t n = (size_t)w * (size_t)h * 3u;
    unsigned char *pixels = (unsigned char *)malloc(n);
    if (!pixels) {
        fclose(fp);
        return 0;
    }

    size_t rd = fread(pixels, 1, n, fp);
    fclose(fp);
    if (rd != n) {
        free(pixels);
        return 0;
    }

    *out_pixels = pixels;
    *out_w = w;
    *out_h = h;
    return 1;
}

static void intro_load_frame_to_texture(int idx) {
    if (idx < 0 || intro_frame_count <= 0) return;
    if (intro_extract_failed) return;

    char path[512];
    snprintf(path, sizeof(path), "video/intro_frames/frame_%05d.ppm", idx);

    unsigned char *pixels = NULL;
    int w = 0, h = 0;
    if (!intro_load_ppm_rgb(path, &pixels, &w, &h)) return;

    if (!intro_tex) {
        glGenTextures(1, &intro_tex);
    }

    glBindTexture(GL_TEXTURE_2D, intro_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    free(pixels);
    intro_last_idx = idx;
}

static void draw_tela_intro_video(void) {
    intro_ensure_frames_extracted();

    if (intro_extract_failed || intro_frame_count <= 0) {
        glColor3f(0.1f, 0.1f, 0.1f);
        glBegin(GL_QUADS);
        glVertex2f(0.f, 0.f);
        glVertex2f(LARGURA_JANELA, 0.f);
        glVertex2f(LARGURA_JANELA, ALTURA_JANELA);
        glVertex2f(0.f, ALTURA_JANELA);
        glEnd();

        float cx = LARGURA_JANELA * 0.5f;
        float cy = ALTURA_JANELA * 0.80f;
        glColor3f(1.f, 1.f, 1.f);
        draw_text_centered(cx, cy, "Pressione ENTER pra iniciar", GLUT_BITMAP_HELVETICA_18);
        return;
    }

    int idx = (frame / 4) % intro_frame_count; /* ~15fps (60fps -> /4) */
    if (idx != intro_last_idx) {
        intro_load_frame_to_texture(idx);
    }

    if (intro_tex) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, intro_tex);
        glColor3f(1.f, 1.f, 1.f);
        glBegin(GL_QUADS);
            /* V invertida pois PPM costuma vir de cima -> baixo */
            glTexCoord2f(0.f, 1.f); glVertex2f(0.f, 0.f);
            glTexCoord2f(1.f, 1.f); glVertex2f(LARGURA_JANELA, 0.f);
            glTexCoord2f(1.f, 0.f); glVertex2f(LARGURA_JANELA, ALTURA_JANELA);
            glTexCoord2f(0.f, 0.f); glVertex2f(0.f, ALTURA_JANELA);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }

    /* Barra escura atrás do texto */
    float cx = LARGURA_JANELA * 0.5f;
    float cy = ALTURA_JANELA * 0.20f;
    float bw = 420.f, bh = 30.f;
    glColor4f(0.f, 0.f, 0.f, 0.45f);
    glBegin(GL_QUADS);
    glVertex2f(cx - bw * 0.5f, cy - bh * 0.5f);
    glVertex2f(cx + bw * 0.5f, cy - bh * 0.5f);
    glVertex2f(cx + bw * 0.5f, cy + bh * 0.5f);
    glVertex2f(cx - bw * 0.5f, cy + bh * 0.5f);
    glEnd();

    glColor3f(1.f, 1.f, 1.f);
    draw_text_centered(cx, cy, "Pressione ENTER pra iniciar", GLUT_BITMAP_HELVETICA_18);
}

 /* ─────────────────────────────────────────
    TELA DE ESCOLHA DE COR
 ───────────────────────────────────────── */
 static void draw_tela_cor(void) {
    float offx = (LARGURA_JANELA - 1024.f) * 0.5f;
    float offy = (ALTURA_JANELA - 700.f) * 0.5f;
    float centro_x = 1024.f * 0.5f;
    float team_gap = 220.f;
    float team_center[2] = {centro_x - team_gap, centro_x + team_gap};
    float ox_base[2] = {team_center[0] - 108.f, team_center[1] - 108.f};
    float btn_w1 = 210.f, btn_w2 = 210.f, btn_w3 = 260.f, btn_gap = 24.f;
    float btn_total = btn_w1 + btn_w2 + btn_w3 + btn_gap * 2.f;
    float btn_x1 = (1024.f - btn_total) * 0.5f;
    float btn_x2 = btn_x1 + btn_w1 + btn_gap;
    float btn_x3 = btn_x2 + btn_w2 + btn_gap;

     glBegin(GL_QUADS);
     glColor3f(0.05f,0.12f,0.05f); glVertex2f(0,0);
     glColor3f(0.05f,0.12f,0.05f); glVertex2f(LARGURA_JANELA,0);
     glColor3f(0.02f,0.08f,0.02f); glVertex2f(LARGURA_JANELA,ALTURA_JANELA);
     glColor3f(0.02f,0.08f,0.02f); glVertex2f(0,ALTURA_JANELA);
     glEnd();
 
    glPushMatrix();
    glTranslatef(offx, offy, 0.f);

     glColor3f(0.9f,0.85f,0.1f);
    draw_text(392,695,"FUTEBOL 2D - OpenGL",GLUT_BITMAP_TIMES_ROMAN_24);
     glColor3f(0.7f,0.7f,0.7f);
    draw_text(410,665,"Escolha as cores dos times",GLUT_BITMAP_HELVETICA_18);
 
     char *labels[2]={"TIME A","TIME B"};
     int t;
     for (t=0;t<2;t++) {
        float ox=ox_base[t], oy=560;
         CorTime *ct=&paleta[cor_selecionada[t]];
         glColor3f(ct->r,ct->g,ct->b);
         draw_text(ox+90,oy,labels[t],GLUT_BITMAP_TIMES_ROMAN_24); // nome do time com a cor selecionada
 
         float px=ox+130, py=oy-80; // ajusta a bola com a cor dos Times
         glColor3f(0,0,0);
         glBegin(GL_TRIANGLE_FAN); glVertex2f(px,py);
         int i;
         for(i=0;i<=24;i++){float a=i*2*(float)M_PI/24;glVertex2f(px+cosf(a)*28,py+sinf(a)*28);}
         glEnd();
         glColor3f(ct->r,ct->g,ct->b);
         glBegin(GL_TRIANGLE_FAN); glVertex2f(px,py);
         for(i=0;i<=24;i++){float a=i*2*(float)M_PI/24;glVertex2f(px+cosf(a)*26,py+sinf(a)*26);}
         glEnd();
         glColor3f(ct->r*0.4f,ct->g*0.4f,ct->b*0.4f); glLineWidth(2);
         glBegin(GL_LINE_LOOP);
         for(i=0;i<=24;i++){float a=i*2*(float)M_PI/24;glVertex2f(px+cosf(a)*26,py+sinf(a)*26);}
         glEnd();
         glColor3f(0.95f,0.80f,0.65f);
         glBegin(GL_TRIANGLE_FAN); glVertex2f(px+12,py+5);
         for(i=0;i<=16;i++){float a=i*2*(float)M_PI/16;glVertex2f(px+12+cosf(a)*10,py+5+sinf(a)*10);}
         glEnd();
         glColor3f(1,1,1);
         draw_text(ox+95,oy-45,ct->nome,GLUT_BITMAP_HELVETICA_18);
         glLineWidth(1);
 
         int c;
         for(c=0;c<NUM_CORES;c++){
             float bx=ox+(c%5)*54, by=oy-160-(c/5)*54, br=22;
             CorTime *pc=&paleta[c];
             if(c==cor_selecionada[t]){
                 glColor3f(1,1,0);
                 glBegin(GL_QUADS);
                 glVertex2f(bx-3,by-3);glVertex2f(bx+br*2+3,by-3);
                 glVertex2f(bx+br*2+3,by+br+3);glVertex2f(bx-3,by+br+3);
                 glEnd();
             }
             glColor3f(pc->r,pc->g,pc->b);
             glBegin(GL_QUADS);
             glVertex2f(bx,by);glVertex2f(bx+br*2,by);
             glVertex2f(bx+br*2,by+br);glVertex2f(bx,by+br);
             glEnd();
             glColor3f(0.5f,0.5f,0.5f);
             glBegin(GL_LINE_LOOP);
             glVertex2f(bx,by);glVertex2f(bx+br*2,by);
             glVertex2f(bx+br*2,by+br);glVertex2f(bx,by+br);
             glEnd();
             glColor3f(0.9f,0.9f,0.9f);
         }
     }
 
     glColor3f(0.6f,0.6f,0.6f);
    draw_text(team_center[0]-90.f,300,"Time A: Teclas 1-0 para selecionar cor",GLUT_BITMAP_HELVETICA_12);
    draw_text(team_center[1]-90.f,300,"Time B: Teclas Q-P para selecionar cor",GLUT_BITMAP_HELVETICA_12);
 
     {
        float mx1=btn_x1,my=30,mw=btn_w1,mh=45,mx2=btn_x2;
         if(modo_jogo==MODO_1P) glColor3f(0.9f,0.7f,0.0f);
         else                   glColor3f(0.25f,0.25f,0.25f);
         glBegin(GL_QUADS);
         glVertex2f(mx1,my);glVertex2f(mx1+mw,my);
         glVertex2f(mx1+mw,my+mh);glVertex2f(mx1,my+mh);
         glEnd();
         glColor3f(0.6f,0.4f,0.0f); glLineWidth(2);
         glBegin(GL_LINE_LOOP);
         glVertex2f(mx1,my);glVertex2f(mx1+mw,my);
         glVertex2f(mx1+mw,my+mh);glVertex2f(mx1,my+mh);
         glEnd();
         glColor3f(1,1,1);
         draw_text(mx1+26,my+15,"[ F1 ] 1 JOGADOR",GLUT_BITMAP_HELVETICA_18);
 
         if(modo_jogo==MODO_2P) glColor3f(0.0f,0.6f,0.9f);
         else                   glColor3f(0.25f,0.25f,0.25f);
         glBegin(GL_QUADS);
         glVertex2f(mx2,my);glVertex2f(mx2+mw,my);
         glVertex2f(mx2+mw,my+mh);glVertex2f(mx2,my+mh);
         glEnd();
         glColor3f(0.0f,0.3f,0.6f); glLineWidth(2);
         glBegin(GL_LINE_LOOP);
         glVertex2f(mx2,my);glVertex2f(mx2+mw,my);
         glVertex2f(mx2+mw,my+mh);glVertex2f(mx2,my+mh);
         glEnd();
         glColor3f(1,1,1);
         draw_text(mx2+20,my+15,"[ F2 ] 2 JOGADORES",GLUT_BITMAP_HELVETICA_18);
         glLineWidth(1);
     }
 
    float bx=btn_x3,by=30,bw=btn_w3,bh=45;
     glColor3f(0.1f,0.6f,0.1f);
     glBegin(GL_QUADS);
     glVertex2f(bx,by);glVertex2f(bx+bw,by);
     glVertex2f(bx+bw,by+bh);glVertex2f(bx,by+bh);
     glEnd();
     glColor3f(0.3f,0.9f,0.3f); glLineWidth(2);
     glBegin(GL_LINE_LOOP);
     glVertex2f(bx,by);glVertex2f(bx+bw,by);
     glVertex2f(bx+bw,by+bh);glVertex2f(bx,by+bh);
     glEnd();
     glColor3f(1,1,1);
    draw_text(bx+24,by+15,"ENTER - INICIAR JOGO!",GLUT_BITMAP_HELVETICA_18);
     glLineWidth(1);

    glPopMatrix();
 }
 
 /* ─────────────────────────────────────────
    TELA DE GOL
 ───────────────────────────────────────── */
 static void draw_tela_gol(void) {
    float bw = 620.f, bh = 200.f;
    float bx = (LARGURA_JANELA - bw) * 0.5f;
    float by = (ALTURA_JANELA - bh) * 0.5f;

    float cx = bx + bw * 0.5f;
     float pulse=0.5f+0.5f*sinf(frame*0.15f);
     CorTime *ct=&paleta[cor_selecionada[ultimo_gol]];
     glColor4f(ct->r,ct->g,ct->b,pulse*0.3f);
     glBegin(GL_QUADS);
    glVertex2f(bx,by);glVertex2f(bx+bw,by);
    glVertex2f(bx+bw,by+bh);glVertex2f(bx,by+bh);
     glEnd();

    glColor3f(ct->r,ct->g,ct->b);
    draw_text_centered(cx, by + 150.f, "G O L !", GLUT_BITMAP_TIMES_ROMAN_24);
     char buf[64];
     snprintf(buf,sizeof(buf),"Time %s marca!",ultimo_gol==0?"A":"B");

     glColor3f(1,1,1);
    draw_text_centered(cx, by + 110.f, buf, GLUT_BITMAP_HELVETICA_18);

     snprintf(buf,sizeof(buf),"%d  :  %d",placar[0],placar[1]);
    glColor3f(1,1,0.2f);
    draw_text_centered(cx, by + 70.f, buf, GLUT_BITMAP_TIMES_ROMAN_24);

    glColor3f(0.85f,0.85f,0.85f);
    draw_text_centered(cx, by + 20.f, "Pressione ENTER para continuar", GLUT_BITMAP_HELVETICA_12);
 }
 
 /* ─────────────────────────────────────────
    TELA DE INTERVALO
 ───────────────────────────────────────── */
 static void draw_tela_intervalo(void) {
    float bw = 624.f, bh = 240.f;
    float bx = (LARGURA_JANELA - bw) * 0.5f;
    float by = (ALTURA_JANELA - bh) * 0.5f;
   float cx = bx + bw * 0.5f;
     glColor4f(0,0,0,0.55f);
     glBegin(GL_QUADS);
     glVertex2f(0,0);glVertex2f(LARGURA_JANELA,0);
     glVertex2f(LARGURA_JANELA,ALTURA_JANELA);glVertex2f(0,ALTURA_JANELA);
     glEnd();
 
     glColor4f(0.1f,0.1f,0.1f,0.8f);
     glBegin(GL_QUADS);
    glVertex2f(bx,by);glVertex2f(bx+bw,by);
    glVertex2f(bx+bw,by+bh);glVertex2f(bx,by+bh);
     glEnd();
 
     glColor3f(1.f, 0.9f, 0.2f);
   draw_text_centered(cx,by + 130.f,"FIM DO 1o TEMPO",GLUT_BITMAP_TIMES_ROMAN_24);
     glColor3f(1.f, 1.f, 1.f);
   draw_text_centered(cx,by + 90.f,"INICIO DO 2o TEMPO",GLUT_BITMAP_HELVETICA_18);
 }
 
 /* ─────────────────────────────────────────
    TELA DE FIM DE JOGO
 ───────────────────────────────────────── */
 static void draw_tela_fim_jogo(void) {
     char buf[96];
    float bw = 624.f, bh = 240.f;
    float bx = (LARGURA_JANELA - bw) * 0.5f;
    float by = (ALTURA_JANELA - bh) * 0.5f;
   float cx = bx + bw * 0.5f;
     glColor4f(0,0,0,0.65f);
     glBegin(GL_QUADS);
     glVertex2f(0,0);glVertex2f(LARGURA_JANELA,0);
     glVertex2f(LARGURA_JANELA,ALTURA_JANELA);glVertex2f(0,ALTURA_JANELA);
     glEnd();
 
     glColor4f(0.08f,0.08f,0.08f,0.86f);
     glBegin(GL_QUADS);
    glVertex2f(bx,by);glVertex2f(bx+bw,by);
    glVertex2f(bx+bw,by+bh);glVertex2f(bx,by+bh);
     glEnd();
 
     glColor3f(1.f, 0.85f, 0.2f);
   draw_text_centered(cx,by + 190.f,"FIM DE JOGO",GLUT_BITMAP_TIMES_ROMAN_24);
 
     if (placar[0] == placar[1]) {
         glColor3f(1.f, 1.f, 1.f);
       draw_text_centered(cx,by + 145.f,"EMPATE",GLUT_BITMAP_HELVETICA_18);
     } else {
         int vencedor = (placar[0] > placar[1]) ? 0 : 1;
         CorTime *ct = &paleta[cor_selecionada[vencedor]];
         snprintf(buf,sizeof(buf),"Vencedor: Time %s",vencedor == 0 ? "A" : "B");
         glColor3f(ct->r,ct->g,ct->b);
       draw_text_centered(cx,by + 145.f,buf,GLUT_BITMAP_HELVETICA_18);
     }
 
     glColor3f(1.f,1.f,0.2f);
     snprintf(buf,sizeof(buf),"%d  :  %d",placar[0],placar[1]);
   draw_text_centered(cx,by + 110.f,buf,GLUT_BITMAP_TIMES_ROMAN_24);
    if (tempo_fim_jogo > 0) {
        int seg = (tempo_fim_jogo + 59) / 60;
        if (seg < 0) seg = 0;
        glColor3f(0.85f,0.85f,0.85f);
      draw_text_centered(cx,by + 70.f,"Aguarde o fim da contagem para ir para a intro",GLUT_BITMAP_HELVETICA_12);
        snprintf(buf,sizeof(buf),"Tempo restante: %ds", seg);
        glColor3f(0.72f,0.72f,0.72f);
       draw_text_centered(cx,by + 48.f,buf,GLUT_BITMAP_HELVETICA_12);
    } else {
        glColor3f(0.8f,0.8f,0.8f);
      draw_text_centered(cx,by + 58.f,"Indo para a intro...",GLUT_BITMAP_HELVETICA_12);
    }
}

static void voltar_tela_intro(void) {
    estado = TELA_INTRO;
    placar[0] = placar[1] = 0;

    tempo_jogo = 0;
    tempo_gol = 0;
    tempo_intervalo = 0;
    tempo_fim_jogo = 0;

    saida_centro_timer = 0;
    posse_inicial_saida = -1;
    time_saida_kickoff = 0;
    gol_crowd_boost = 0;

    etapa_atual = 1;
    lados_invertidos = 0;
    ultimo_gol = -1;

    reposicao_tipo = REPOSICAO_NENHUMA;
    reposicao_time = -1;
    reposicao_timer = 0;
    reposicao_jogador = -1;
    reposicao_timer_humano = 0;
    reposicao_apito_timer = 0;
    saida_apito_timer = 0;
    ultimo_toque = -1;
    bola.posse = -1;

    frame = 0;
    intro_last_idx = -1;
}

static void voltar_tela_cor(void) {
    estado = TELA_COR;
    placar[0] = placar[1] = 0;
    tempo_jogo = 0;
    tempo_gol = 0;
    tempo_intervalo = 0;
    tempo_fim_jogo = 0;
    saida_centro_timer = 0;
    posse_inicial_saida = -1;
    gol_crowd_boost = 0;
    etapa_atual = 1;
    lados_invertidos = 0;
    ultimo_gol = -1;
    reposicao_tipo = REPOSICAO_NENHUMA;
    reposicao_time = -1;
    reposicao_timer = 0;
    reposicao_jogador = -1;
    reposicao_timer_humano = 0;
    reposicao_apito_timer = 0;
    saida_apito_timer = 0;
    ultimo_toque = -1;
    bola.posse = -1;
 }
 
 /* ─────────────────────────────────────────
    DISPLAY
 ───────────────────────────────────────── */
 static void display(void) {
     glClearColor(0.1f,0.1f,0.1f,1);
     glClear(GL_COLOR_BUFFER_BIT);
     glMatrixMode(GL_PROJECTION); glLoadIdentity();
     gluOrtho2D(0,LARGURA_JANELA,0,ALTURA_JANELA);
     glMatrixMode(GL_MODELVIEW); glLoadIdentity();
 
     if (estado==TELA_COR) {
         draw_tela_cor();
    } else if (estado==TELA_INTRO) {
        draw_tela_intro_video();
     } else {
         draw_arquibancada_e_torcida();
         draw_campo();
         int t, j;
         for(t=0;t<2;t++){
             CorTime *ct=&paleta[cor_selecionada[t]];
             for(j=0;j<NUM_JOGADORES;j++){
                 int is_hum = (jogador_humano[t]==j &&
                               (t==0 || modo_jogo==MODO_2P));
                 draw_jogador(&jogadores[t][j],ct->r,ct->g,ct->b,is_hum);
             }
         }
         draw_bola();
         /* Indicador de passe */
         draw_passe_visual(0);
         if (modo_jogo == MODO_2P) draw_passe_visual(1);
         draw_hud();
         if (estado==INTERVALO) draw_tela_intervalo();
         if (estado==GOL_MARCADO) draw_tela_gol();
         if (estado==FIM_JOGO) draw_tela_fim_jogo();
     }
     glutSwapBuffers();
 }
 
 /* ─────────────────────────────────────────
    UPDATE
 ───────────────────────────────────────── */
 static void update(int val) {
     (void)val;
     frame++;
     if (audio_danger_cd > 0) audio_danger_cd--;
     if (gol_crowd_boost > 0) gol_crowd_boost--;

    /* Intro + seleção: tocar apenas audios/intro.mp3 */
    {
        static int intro_audio_on = 0;
        int want_intro = (estado == TELA_INTRO || estado == TELA_COR);
        if (want_intro && !intro_audio_on) {
            audio_stop_intro();
            audio_set_crowd_active(0);
            audio_play_intro();
            intro_audio_on = 1;
        } else if (!want_intro && intro_audio_on) {
            audio_stop_intro();
            audio_set_crowd_active(1);
            intro_audio_on = 0;
        }
    }
 
     if (estado==JOGANDO) {
        if (saida_apito_timer > 0) {
            saida_apito_timer--;
            if (saida_apito_timer == 0) audio_play_apito_longo();
            audio_tick_crowd((int)estado, gol_crowd_boost > 0);
            glutPostRedisplay();
            glutTimerFunc(16,update,0);
            return;
        }

         tempo_jogo++;
         if (tempo_jogo >= FRAMES_PARTIDA) {
             estado = FIM_JOGO;
            tempo_fim_jogo = 600; /* 10 segundos */
         } else if (tempo_jogo >= FRAMES_POR_TEMPO && etapa_atual == 1) {
             estado = INTERVALO;
             tempo_intervalo = 180;
         }
 
         int t, j;
         for(t=0;t<2;t++){
             int humano = (t==0) ? 1 : (modo_jogo==MODO_2P ? 1 : 0);
             if(humano){
                 atualizar_troca_humano_sem_bola(t);
                 controle_humano(t);
                 atribuir_marcacoes(t);
                 for(j=0;j<NUM_JOGADORES;j++){
                     if(j==jogador_humano[t]) continue;
                     ia_companheiro(t,j);
                 }
             } else {
                 for(j=0;j<NUM_JOGADORES;j++)
                     ia_jogador_full(t,j);
             }
         }
         atualiza_bola();
         if (saida_centro_timer > 0) {
             if (bola.posse != posse_inicial_saida || bola.posse < 0)
                 saida_centro_timer = 0;
             else
                 saida_centro_timer--;
         }
     } else if (estado==INTERVALO) {
         tempo_intervalo--;
         if (tempo_intervalo <= 0) {
             etapa_atual = 2;
             lados_invertidos = !lados_invertidos; /* troca de lado */
             posicionar_times_reinicio(1, 0, 1);   /* segundo tempo: time 1 na saída */
             estado = JOGANDO;
         }
     } else if(estado==GOL_MARCADO){
         tempo_gol--;
         if(tempo_gol<=0){ init_jogo(); estado=JOGANDO; }
    } else if(estado==FIM_JOGO){
        tempo_fim_jogo--;
        if (tempo_fim_jogo <= 0) {
            voltar_tela_intro();
        }
     }
 
    if (estado != TELA_INTRO && estado != TELA_COR) {
        audio_tick_crowd((int)estado, gol_crowd_boost > 0);
    }
 
     glutPostRedisplay();
     glutTimerFunc(16,update,0);
 }
 
 /* ─────────────────────────────────────────
    TECLADO
 ───────────────────────────────────────── */
 static void keyboard(unsigned char key, int x, int y) {
     (void)x;(void)y;
     teclas[key]=1;
    if (estado == TELA_INTRO && key == 13) {
        voltar_tela_cor();
        return;
    }
    if (estado == FIM_JOGO && tempo_fim_jogo == 0 && key == 13) {
        voltar_tela_intro();
        return;
    }
     if(estado==TELA_COR){
         if(key>='1'&&key<='9') cor_selecionada[0]=key-'1';
         if(key=='0') cor_selecionada[0]=9;
         char mapa[]="qwertyuiop";
         int m;
         for(m=0;m<10;m++) if(key==mapa[m]) cor_selecionada[1]=m;
         if(key==13){ init_jogo(); estado=JOGANDO; }
     }
     if(key==27) exit(0);
 }
 static void keyboard_up(unsigned char key, int x, int y){
     (void)x;(void)y; teclas[key]=0;
 }
 static void special(int key, int x, int y){
     (void)x;(void)y;
     teclas_esp[key]=1;
     if(estado==TELA_COR){
         if(key==GLUT_KEY_F1) modo_jogo=MODO_1P;
         if(key==GLUT_KEY_F2) modo_jogo=MODO_2P;
     }
 }
 static void special_up(int key, int x, int y){
     (void)x;(void)y; teclas_esp[key]=0;
 }
 
 /* ─────────────────────────────────────────
    MOUSE
 ───────────────────────────────────────── */
 static void mouse(int button, int mstate, int mx, int my){
     if(button!=GLUT_LEFT_BUTTON||mstate!=GLUT_DOWN) return;
   if(estado==FIM_JOGO) return;
     if(estado!=TELA_COR) return;
    {
        float offx = (LARGURA_JANELA - 1024.f) * 0.5f;
        float offy = (ALTURA_JANELA - 700.f) * 0.5f;
        float centro_x = 1024.f * 0.5f;
        float team_gap = 220.f;
        float team_center[2] = {centro_x - team_gap, centro_x + team_gap};
        float ox_base[2] = {team_center[0] - 108.f, team_center[1] - 108.f};
        float btn_w1 = 210.f, btn_w2 = 210.f, btn_w3 = 260.f, btn_gap = 24.f;
        float btn_total = btn_w1 + btn_w2 + btn_w3 + btn_gap * 2.f;
        float btn_x1 = (1024.f - btn_total) * 0.5f;
        float btn_x2 = btn_x1 + btn_w1 + btn_gap;
        float btn_x3 = btn_x2 + btn_w2 + btn_gap;
        float wy=ALTURA_JANELA-my;
        float ux = mx - offx;
        float uy = wy - offy;
     int t;
     for(t=0;t<2;t++){
        float ox=ox_base[t], oy=560;
         int c;
         for(c=0;c<NUM_CORES;c++){
             float bx=ox+(c%5)*54, by=oy-160-(c/5)*54, br=22;
            if(ux>=bx&&ux<=bx+br*2&&uy>=by&&uy<=by+br){
                 cor_selecionada[t]=c; return;
             }
         }
     }
    if(uy>=30&&uy<=75){
        if(ux>=btn_x1&&ux<=btn_x1+btn_w1){ modo_jogo=MODO_1P; return; }
        if(ux>=btn_x2&&ux<=btn_x2+btn_w2){ modo_jogo=MODO_2P; return; }
     }
    if(ux>=btn_x3&&ux<=btn_x3+btn_w3&&uy>=30&&uy<=75){ init_jogo(); estado=JOGANDO; }
    }
 }
 
 /* ─────────────────────────────────────────
    MAIN
 ───────────────────────────────────────── */
 int main(int argc, char **argv){
     glutInit(&argc,argv);
     glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
     glutInitWindowSize(LARGURA_JANELA,ALTURA_JANELA);
     glutInitWindowPosition(100,50);
     glutCreateWindow("Futebol 2D - OpenGL");
 
     glEnable(GL_BLEND);
     glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
     glEnable(GL_LINE_SMOOTH);
     glHint(GL_LINE_SMOOTH_HINT,GL_NICEST);
     audio_init();
 
     memset(teclas,0,sizeof(teclas));
     memset(teclas_esp,0,sizeof(teclas_esp));
 
     glutDisplayFunc(display);
     glutKeyboardFunc(keyboard);
     glutKeyboardUpFunc(keyboard_up);
     glutSpecialFunc(special);
     glutSpecialUpFunc(special_up);
     glutMouseFunc(mouse);
     glutTimerFunc(16,update,0);
 
     printf("=== FUTEBOL 2D ===\n");
    if (audio_is_available()){
        printf("Audio: OpenAL ativo\n");
    }
    else{
        printf("Compilar (com som OpenAL): gcc fut.c audio.c -o fut -lGL -lGLU -lglut -lopenal -lm\n");
        printf("Audio: OpenAL indisponivel (instale openal-soft-devel)\n");
    }
     glutMainLoop();
     return 0;
 }