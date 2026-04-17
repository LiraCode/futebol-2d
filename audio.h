#ifndef AUDIO_H
#define AUDIO_H

/* OpenAL: inicialização e efeitos. Sons gerados em tempo de execução (PCM). */
void audio_init(void);
void audio_shutdown(void);

void audio_play_kick(void);
void audio_play_gol(void);
void audio_play_perigo(void);
void audio_play_apito_curto(void);
void audio_play_apito_longo(void);

/* Intro / trilha de fundo (Loop) */
void audio_play_intro(void);
void audio_stop_intro(void);

/* Controla a torcida (crowd) para silenciar em telas */
void audio_set_crowd_active(int active);

/* Torcida: volume base 0..1 durante o jogo; reforço automático em gol */
void audio_tick_crowd(int estado_jogo, int em_gol_recente);
int audio_is_available(void);

#endif
