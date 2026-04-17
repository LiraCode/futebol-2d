/*
 * Áudio com OpenAL + MP3 (decodificado por ffmpeg em runtime).
 * Se OpenAL não existir, as funções viram no-op.
 */
#include "audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#  if __has_include(<AL/al.h>) && __has_include(<AL/alc.h>)
#    include <AL/al.h>
#    include <AL/alc.h>
#    define AUDIO_OPENAL 1
#  endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef AUDIO_OPENAL

void audio_init(void) {}
void audio_shutdown(void) {}
void audio_play_kick(void) {}
void audio_play_gol(void) {}
void audio_play_perigo(void) {}
void audio_play_apito_curto(void) {}
void audio_play_apito_longo(void) {}
void audio_tick_crowd(int estado_jogo, int em_gol_recente) { (void)estado_jogo; (void)em_gol_recente; }
int audio_is_available(void) { return 0; }

#else

#define SR 22050
#define N_SFX 10
#define SFX_GAIN 2.4f
/* Ajuste fino da torcida: 3.0f (alto), 4.0f (bem alto), 5.0f (extremo) */
#define CROWD_GAIN_MUL 10.0f
#define CROWD_MAX_TRACK_FRAMES (30 * 60) /* máximo de 30s por faixa */

static ALCdevice  *al_dev;
static ALCcontext *al_ctx;
static int         al_ok;

static ALuint buf_kick, buf_gol, buf_perigo, buf_apito_c, buf_apito_l;
static ALuint buf_crowd_amb, buf_crowd_vocal, buf_crowd_press;
static ALuint src_crowd;
static ALuint src_goal;
static ALuint src_sfx[N_SFX];
static ALuint buf_intro = 0;
static ALuint src_intro = 0;
static int    intro_loaded = 0;
static int    sfx_i;
static int    crowd_swap_cd;
static int    crowd_track_timer;
static ALuint crowd_current;
static int    crowd_loaded;
static int    goal_from_file;
static float  crowd_last_offset;

static short *alloc_samples(int n) {
    return (short *)calloc((size_t)n, sizeof(short));
}

static void upload_buf(ALuint *id, const short *s, int n_samples) {
    alGenBuffers(1, id);
    alBufferData(*id, AL_FORMAT_MONO16, s, n_samples * (int)sizeof(short), SR);
}

static void upload_from_bytes(ALuint *id, const unsigned char *s, int n_bytes) {
    alGenBuffers(1, id);
    alBufferData(*id, AL_FORMAT_MONO16, s, n_bytes, SR);
}

static void gen_kick(short *s, int n) {
    int i;
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = expf(-t * 18.f);
        float x = ((float)rand() / (float)RAND_MAX) * 2.f - 1.f;
        s[i] = (short)(x * env * 28000.f);
    }
}

static void gen_gol(short *s, int n) {
    int i;
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)SR;
        float f = 400.f + 650.f * (1.f - expf(-t * 4.f));
        float e = expf(-t * 1.1f);
        float w = sinf(2.f * (float)M_PI * f * t) * 0.55f
                + sinf(2.f * (float)M_PI * f * 2.1f * t) * 0.25f;
        float nn = ((float)rand() / (float)RAND_MAX) * 2.f - 1.f;
        s[i] = (short)((w * 0.85f + nn * 0.12f) * e * 30000.f);
    }
}

static void gen_perigo(short *s, int n) {
    int i;
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float env = sinf((float)M_PI * t);
        float w = sinf(2.f * (float)M_PI * 1950.f * (float)i / (float)SR);
        s[i] = (short)(w * env * 26000.f);
    }
}

static void gen_apito(short *s, int n, int longo) {
    int i;
    float dur = (float)n / (float)SR;
    for (i = 0; i < n; i++) {
        float t = (float)i / (float)SR;
        float env = longo ? (1.f - expf(-t * 25.f)) * expf(-fmaxf(0.f, t - dur * 0.72f) * 8.f)
                          : (1.f - expf(-t * 40.f)) * expf(-(t / dur) * 5.f);
        float tr = sinf(2.f * (float)M_PI * 7.f * t);
        float f = 2850.f + 180.f * tr;
        float w = sinf(2.f * (float)M_PI * f * t) * 0.65f
                + sinf(2.f * (float)M_PI * f * 1.7f * t) * 0.35f;
        s[i] = (short)(w * env * 24000.f);
    }
}

static void gen_crowd_loop(short *s, int n) {
    int i;
    float carry = 0.f;
    for (i = 0; i < n; i++) {
        float x = ((float)rand() / (float)RAND_MAX) * 2.f - 1.f;
        carry = carry * 0.92f + x * 0.08f;
        float t = (float)i / (float)n;
        float w = 0.5f - 0.5f * cosf(2.f * (float)M_PI * t);
        s[i] = (short)(carry * w * 12000.f);
    }
}

static int load_mp3_to_buffer(const char *path, ALuint *out) {
    char cmd[1024];
    unsigned char *pcm = NULL;
    int cap = 0, sz = 0;
    FILE *fp;

    snprintf(cmd, sizeof(cmd),
             "ffmpeg -v error -nostdin -i \"%s\" -f s16le -acodec pcm_s16le -ac 1 -ar %d -",
             path, SR);
    fp = popen(cmd, "r");
    if (!fp) return 0;

    for (;;) {
        unsigned char chunk[4096];
        size_t n = fread(chunk, 1, sizeof(chunk), fp);
        if (n > 0) {
            if (sz + (int)n > cap) {
                int ncap = (cap == 0) ? 16384 : cap * 2;
                while (ncap < sz + (int)n) ncap *= 2;
                pcm = (unsigned char *)realloc(pcm, (size_t)ncap);
                if (!pcm) { pclose(fp); return 0; }
                cap = ncap;
            }
            memcpy(pcm + sz, chunk, n);
            sz += (int)n;
        }
        if (n < sizeof(chunk)) break;
    }
    pclose(fp);
    if (sz <= 0 || !pcm) { free(pcm); return 0; }

    upload_from_bytes(out, pcm, sz);
    free(pcm);
    return 1;
}

static void play_buffer(ALuint b) {
    ALuint src;
    if (!b) return;
    src = src_sfx[sfx_i];
    sfx_i = (sfx_i + 1) % N_SFX;
    alSourceStop(src);
    alSourcei(src, AL_BUFFER, (ALint)b);
    alSourcef(src, AL_GAIN, SFX_GAIN);
    alSourcePlay(src);
}

static void crowd_set_buffer(ALuint b) {
    if (!b || b == crowd_current) return;
    crowd_current = b;
    alSourceStop(src_crowd);
    alSourcei(src_crowd, AL_BUFFER, (ALint)b);
    alSourcei(src_crowd, AL_LOOPING, AL_TRUE);
    alSourcePlay(src_crowd);
    crowd_last_offset = 0.f;
    crowd_track_timer = CROWD_MAX_TRACK_FRAMES;
}

static ALuint crowd_pick_random_next(void) {
    ALuint opts[3];
    int n = 0;
    if (buf_crowd_amb && buf_crowd_amb != crowd_current) opts[n++] = buf_crowd_amb;
    if (buf_crowd_vocal && buf_crowd_vocal != crowd_current) opts[n++] = buf_crowd_vocal;
    if (buf_crowd_press && buf_crowd_press != crowd_current) opts[n++] = buf_crowd_press;
    if (n <= 0) return crowd_current;
    return opts[rand() % n];
}

static int crowd_completed_full_cycle(void) {
    float off = 0.f;
    int wrapped = 0;
    alGetSourcef(src_crowd, AL_SEC_OFFSET, &off);
    if (crowd_last_offset > 1.f && off < (crowd_last_offset - 0.25f)) wrapped = 1;
    crowd_last_offset = off;
    return wrapped;
}

void audio_init(void) {
    int i;
    al_ok = 0;
    crowd_loaded = 0;
    al_dev = alcOpenDevice(NULL);
    if (!al_dev) return;
    al_ctx = alcCreateContext(al_dev, NULL);
    if (!al_ctx || !alcMakeContextCurrent(al_ctx)) {
        if (al_ctx) alcDestroyContext(al_ctx);
        alcCloseDevice(al_dev);
        al_dev = NULL;
        return;
    }

    /* SFX MP3 (com fallback procedural quando faltar arquivo). */
    if (!load_mp3_to_buffer("audios/chute.mp3", &buf_kick)) {
        short *tmp = alloc_samples(SR / 9);
        gen_kick(tmp, SR / 9);
        upload_buf(&buf_kick, tmp, SR / 9);
        free(tmp);
    }
    if (!load_mp3_to_buffer("audios/gol.mp3", &buf_gol)) {
        short *tmp = alloc_samples((int)(SR * 0.75f));
        gen_gol(tmp, (int)(SR * 0.75f));
        upload_buf(&buf_gol, tmp, (int)(SR * 0.75f));
        free(tmp);
        goal_from_file = 0;
    } else {
        goal_from_file = 1;
    }
    if (!load_mp3_to_buffer("audios/escateio.mp3", &buf_perigo)) {
        short *tmp = alloc_samples(SR / 5);
        gen_perigo(tmp, SR / 5);
        upload_buf(&buf_perigo, tmp, SR / 5);
        free(tmp);
    }
    if (!load_mp3_to_buffer("audios/lateral.mp3", &buf_apito_c)) {
        short *tmp = alloc_samples(SR / 4);
        gen_apito(tmp, SR / 4, 0);
        upload_buf(&buf_apito_c, tmp, SR / 4);
        free(tmp);
    }
    if (!load_mp3_to_buffer("audios/startgol.mp3", &buf_apito_l)) {
        short *tmp = alloc_samples((int)(SR * 0.55f));
        gen_apito(tmp, (int)(SR * 0.55f), 1);
        upload_buf(&buf_apito_l, tmp, (int)(SR * 0.55f));
        free(tmp);
    }

    /* Torcida: alterna entre leves e usa "pressão" em lances críticos. */
    if (load_mp3_to_buffer("audios/tocidaAmbiance.mp3", &buf_crowd_amb) &&
        load_mp3_to_buffer("audios/torcida normal.mp3", &buf_crowd_vocal) &&
        load_mp3_to_buffer("audios/bateria.mp3", &buf_crowd_press)) {
        crowd_loaded = 1;
    } else {
        short *tmp = alloc_samples(SR * 3);
        gen_crowd_loop(tmp, SR * 3);
        upload_buf(&buf_crowd_amb, tmp, SR * 3);
        upload_buf(&buf_crowd_vocal, tmp, SR * 3);
        upload_buf(&buf_crowd_press, tmp, SR * 3);
        free(tmp);
    }

    alGenSources(1, &src_crowd);
    alGenSources(1, &src_goal);
    alSourcef(src_crowd, AL_MAX_GAIN, 12.0f);
    crowd_swap_cd = SR * 12 / 60;
    crowd_current = 0;
    crowd_last_offset = 0.f;
    crowd_track_timer = CROWD_MAX_TRACK_FRAMES;
    crowd_set_buffer(buf_crowd_amb);
    alSourcef(src_crowd, AL_GAIN, 0.80f);

    /* Intro mp3 (loop) */
    intro_loaded = load_mp3_to_buffer("audios/intro.mp3", &buf_intro);
    if (intro_loaded) {
        alGenSources(1, &src_intro);
        alSourcef(src_intro, AL_GAIN, 0.95f);
        alSourcei(src_intro, AL_BUFFER, (ALint)buf_intro);
        alSourcei(src_intro, AL_LOOPING, AL_TRUE);
        alSourceStop(src_intro);
    }

    for (i = 0; i < N_SFX; i++) alGenSources(1, &src_sfx[i]);
    sfx_i = 0;
    al_ok = 1;
}

void audio_shutdown(void) {
    int k;
    if (!al_ok) return;
    alSourceStop(src_crowd);
    alSourceStop(src_goal);
    if (intro_loaded) {
        alSourceStop(src_intro);
        alDeleteSources(1, &src_intro);
        alDeleteBuffers(1, &buf_intro);
    }
    for (k = 0; k < N_SFX; k++) alSourceStop(src_sfx[k]);
    alDeleteSources(1, &src_crowd);
    alDeleteSources(1, &src_goal);
    alDeleteSources(N_SFX, src_sfx);
    alDeleteBuffers(1, &buf_kick);
    alDeleteBuffers(1, &buf_gol);
    alDeleteBuffers(1, &buf_perigo);
    alDeleteBuffers(1, &buf_apito_c);
    alDeleteBuffers(1, &buf_apito_l);
    alDeleteBuffers(1, &buf_crowd_amb);
    alDeleteBuffers(1, &buf_crowd_vocal);
    alDeleteBuffers(1, &buf_crowd_press);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(al_ctx);
    alcCloseDevice(al_dev);
    al_ctx = NULL;
    al_dev = NULL;
    al_ok = 0;
}

void audio_play_kick(void) { if (al_ok) play_buffer(buf_kick); }
void audio_play_gol(void) {
    if (!al_ok) return;
    if (goal_from_file) {
        alSourceStop(src_goal);
        alSourcei(src_goal, AL_BUFFER, (ALint)buf_gol);
        alSourcef(src_goal, AL_GAIN, SFX_GAIN);
        alSourcePlay(src_goal);
    } else {
        play_buffer(buf_gol);
    }
}
void audio_play_perigo(void) { if (al_ok) play_buffer(buf_perigo); }
void audio_play_apito_curto(void) { if (al_ok) play_buffer(buf_apito_c); }
void audio_play_apito_longo(void) { if (al_ok) play_buffer(buf_apito_l); }

void audio_play_intro(void) {
    if (!al_ok || !intro_loaded) return;
    alSourceStop(src_intro);
    alSourcei(src_intro, AL_BUFFER, (ALint)buf_intro);
    alSourcei(src_intro, AL_LOOPING, AL_TRUE);
    alSourcef(src_intro, AL_GAIN, 0.95f);
    alSourcePlay(src_intro);
}

void audio_stop_intro(void) {
    if (!al_ok || !intro_loaded) return;
    alSourceStop(src_intro);
}

void audio_set_crowd_active(int active) {
    if (!al_ok) return;
    if (active) {
        /* volta a tocar se parou */
        alSourcePlay(src_crowd);
    } else {
        alSourceStop(src_crowd);
    }
}

void audio_tick_crowd(int estado_jogo, int em_gol_recente) {
    float g;
    int force_swap_30s;
    int cycle_finished;
    if (!al_ok) return;

    if (estado_jogo == 0) {
        g = 0.06f;
    } else if (em_gol_recente) {
        g = 0.72f;
    } else if (estado_jogo == 1 || estado_jogo == 3) {
        g = 0.28f;
    } else if (estado_jogo == 2) {
        g = 0.35f;
    } else if (estado_jogo == 4) {
        g = 0.12f;
    } else {
        g = 0.18f;
    }
    g *= CROWD_GAIN_MUL;
    alSourcef(src_crowd, AL_GAIN, g);
    if (crowd_track_timer > 0) crowd_track_timer--;
    force_swap_30s = (crowd_track_timer <= 0);

    (void)em_gol_recente;

    cycle_finished = crowd_completed_full_cycle();
    if (!force_swap_30s && !cycle_finished) return;
    if (crowd_swap_cd > 0) {
        crowd_swap_cd--;
        return;
    }

    /* Troca aleatória de torcida no fim do ciclo ou ao atingir 30s. */
    crowd_set_buffer(crowd_pick_random_next());
    crowd_swap_cd = 1;
}

int audio_is_available(void) {
    return al_ok;
}

#endif /* AUDIO_OPENAL */
