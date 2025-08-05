#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <aubio/aubio.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <time.h>

struct tmproc {
    // Pipewire stuff
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_hook listener;
    struct pw_context *context;
    struct pw_core *core;
    // Time start (since process start)
    struct timeval start_time;
    // Aubio tempo
    smpl_t is_beat;
    uint_t is_silence;
    aubio_tempo_t *tempo;
    fvec_t *tempo_out;
    fvec_t *tempo_in;
    uint_t buffer_size;
    uint_t hop_size;
    uint_t sample_rate;
    smpl_t silent_treshold;
    // Some stuff for evars
    bool use_pipewire;
    bool json;
    bool no_termclear;
    bool no_metronome;
    // Frame counter for metronome
    int frame_index;
};

volatile sig_atomic_t exit_requested = 0;
struct tmproc *global_tmp = NULL;

void handle_sigint(int sig) {
    exit_requested = 1;
    if (global_tmp && global_tmp->loop) {
        pw_main_loop_quit(global_tmp->loop);
    }
}

void cleanup_tmproc(struct tmproc *tmp) {
    if (tmp->use_pipewire) {
        pw_stream_destroy(tmp->stream);
        pw_core_disconnect(tmp->core);
        pw_context_destroy(tmp->context);
        pw_main_loop_destroy(tmp->loop);
        pw_deinit();
    }
    del_aubio_tempo(tmp->tempo);
    del_fvec(tmp->tempo_in);
    del_fvec(tmp->tempo_out);
    aubio_cleanup();
    fflush(stdin);
    fflush(stderr);
}

static void on_audio_input(struct tmproc *tmp){
    

    aubio_tempo_do(tmp->tempo, tmp->tempo_in, tmp->tempo_out);
    tmp->is_beat = fvec_get_sample(tmp->tempo_out, 0);
    if (tmp->silent_treshold != -90.) {
        tmp->is_silence = aubio_silence_detection(tmp->tempo_in, tmp->silent_treshold);
    }

    if (tmp->is_beat && !tmp->is_silence) {
        float peak = 0.f;
        for (uint_t i = 0; i < tmp->tempo_in->length; i++) {
            float val = fabsf(tmp->tempo_in->data[i]);
            if (val > peak) peak = val;
        }

        struct timeval now;
        gettimeofday(&now, NULL);
        long seconds = now.tv_sec - tmp->start_time.tv_sec;
        long useconds = now.tv_usec - tmp->start_time.tv_usec;
        long mseconds = seconds * 1000 + useconds / 1000;
        float bpm = aubio_tempo_get_bpm(tmp->tempo);
        if (!tmp->json && !tmp->no_termclear)
            printf("\033[2J\033[H");
        if (tmp->json) {
            printf("{\"tact_time\":%ld.%03ld, \"bpm\":%.2f, \"beat_amp\":%.4f, \"metronome\": %d}\n", mseconds / 1000, mseconds % 1000, bpm, peak, (tmp->frame_index % 4)+1);
        } else {
            static const char *frames[] = {"[#]/[ ]", "[ ]-[#]", "[#]\\[ ]", "[ ]|[#]"};
            printf("[!] Got tact at: %ld.%03lds:\n\twith %.2f BPM\n\tand amplitude %.4f\n",
                   mseconds / 1000, mseconds % 1000, bpm, peak);
            if (!tmp->no_metronome)
            printf("%s\n", frames[tmp->frame_index % 4]);    
        }
        tmp->frame_index++;
        fflush(stdout);
    }
}

static void on_pw_process(void *data) {
    struct tmproc *app = data;
    struct pw_buffer *buf = pw_stream_dequeue_buffer(app->stream);
    if (!buf) {
        if (!app->json) printf("[X] No buffer data available.\n");
        return;
    }

    struct spa_data *d = &buf->buffer->datas[0];
    float *audio = (float *)d->data;
    if (!audio) return;

    memcpy(app->tempo_in->data, audio, app->hop_size * sizeof(float));
    
    on_audio_input(app);

    pw_stream_queue_buffer(app->stream, buf);
}

static const struct pw_stream_events pw_stream_events = {
    .process = on_pw_process,
};

void init_pipewire_sink(struct tmproc *tmp) {
    tmp->loop = pw_main_loop_new(NULL);
    tmp->context = pw_context_new(pw_main_loop_get_loop(tmp->loop), NULL, 0);
    tmp->core = pw_context_connect(tmp->context, NULL, 0);

    tmp->stream = pw_stream_new(tmp->core, "TactMon", NULL);
    pw_stream_add_listener(tmp->stream, &tmp->listener, &pw_stream_events, tmp);

    struct spa_audio_info_raw info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .channels = 1,
        .rate = tmp->sample_rate,
        .position = { SPA_AUDIO_CHANNEL_MONO }
    };

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    const struct spa_pod *params[2];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    struct spa_pod_builder b2 = SPA_POD_BUILDER_INIT(buffer + 512, sizeof(buffer) - 512);
    params[1] = spa_pod_builder_add_object(&b2,
        SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers,  SPA_POD_Int(8),
        SPA_PARAM_BUFFERS_blocks,   SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_size,     SPA_POD_Int(tmp->hop_size * sizeof(float)),
        SPA_PARAM_BUFFERS_stride,   SPA_POD_Int(sizeof(float)),
        SPA_PARAM_BUFFERS_align,    SPA_POD_Int(16));

    pw_stream_connect(tmp->stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_MAP_BUFFERS,
        params, 2);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0); 
    struct tmproc tm = {0};
    global_tmp = &tm;

    gettimeofday(&tm.start_time, NULL);

    tm.use_pipewire = true;
    tm.is_silence = 0;
    tm.is_beat = 0.;
    tm.silent_treshold = -90.0;
    tm.sample_rate = 44100;
    tm.buffer_size = 1024;
    tm.hop_size = 512;
    tm.json = false;
    tm.no_termclear = false;
    tm.no_metronome = false;
    tm.frame_index = 0;
    
    
    char_t detection_mode[32] = "default";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input-source=stdin") == 0) {
            tm.use_pipewire = false;
        } else if (strcmp(argv[i], "--json") == 0) {
            tm.json = true;
        } else if (strcmp(argv[i], "--no-clear") == 0) {
            tm.no_termclear = true;
        } else if (strcmp(argv[i], "--no-metronome") == 0) {
            tm.no_metronome = true;
        } else if (strncmp(argv[i], "--tempo-method=", 15) == 0) {
            strncpy((char *)detection_mode, argv[i] + 15, sizeof(detection_mode) - 1);
            detection_mode[sizeof(detection_mode) - 1] = '\0';
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--json] [--no-clear] [--no-metronome] [--tempo-method=default|complex|...] \n", argv[0]);
            return 0;
        }    
    }

    signal(SIGINT, handle_sigint);

    if (tm.use_pipewire) {
        if (!tm.json) printf("[!] Creating PipeWire sink\n");
        pw_init(&argc, &argv);
        init_pipewire_sink(&tm);

        tm.tempo = new_aubio_tempo(detection_mode, tm.buffer_size, tm.hop_size, tm.sample_rate);
        if (!tm.tempo) {
            printf("[X] Unable to initialize aubio tempo\n");
            printf("[X] Did you spell --tempo-method wrong?\n");
            cleanup_tmproc(&tm);
            return -1;
        }
        tm.tempo_in = new_fvec(tm.hop_size);
        tm.tempo_out = new_fvec(2);

        if (!tm.json) printf("[!] Starting PipeWire event loop\n");
        pw_main_loop_run(tm.loop);
    }

    if (!tm.json) printf("\n[!] Cleaning up...\n");
    cleanup_tmproc(&tm);
    if (!tm.json) printf("[!] See you next time!\n");
    return 0;
}

