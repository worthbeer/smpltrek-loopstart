/* Loop-clip START POS sub-page: bar.beat readout + snap-to-beat/bar.   SmplTrek 3.2.174 ONLY.
 * Appended-section payload linked at 0xC3F00000 (link.ld). Hooks = two words of the AppLoopTrackView vtable
 * (vptr 0xC16C5978): handleEncoder (+28, word 0xC16C5994) and draw (+56, word 0xC16C59B0).
 *
 * Facts established in the emulator on the stock firmware (see docs/knowledge-base.md):
 *   - the START POS sub-page is a MODE of AppLoopTrackView: *(DP+300)==1 while it is open (B pressed), 0 after OK/CLR
 *   - *(DP+296) = track index of the clip being edited
 *   - encoder A (index 0) edits the PENDING start in view->+24 (frames @48k); OK commits it
 *   - stock click size = file_frames/128; stock max = file_frames - 3000; file frames = 0xC15C56E4(track)
 *   - after each change the stock handler does: refresh(0xC155F36C), apply(0xC1569680 track,val,1), refresh, preview(0xC1568970 track,val,0.25f)
 * Rules: no globals, no libc/libgcc (divide via firmware __c6xabi_divu), B14 never written except one stw of DP+1240.
 */
typedef unsigned int u32;
typedef int s32;

#define FW_ENC_ORIG      0xC1560350u   // void handleEncoder(void *this, int enc, int delta)
#define FW_DRAW_ORIG     0xC1561554u   // void draw(void *this, void *canvas)
#define FW_STORE         0xC155FF50u   // void store_start(int value)  writes the clip table (via 0xC15BC938); stock base for the next click
#define FW_REFRESH       0xC155F36Cu   // void *refresh(int track)  returns the clip object
#define FW_APPLY         0xC1569680u   // void apply(void *obj, int value, int one)
#define FW_PREVIEW       0xC1568970u   // void preview(void *obj, int value, float quarter)
#define FW_CLIP_LEN      0xC15C56E4u   // unsigned clip_frames(int track)
#define FW_TEMPO_X10     0xC1611428u   // int project_get_tempo_x10(void)
#define FW_DIVU          0xC1690640u   // unsigned __c6xabi_divu(unsigned a, unsigned b)
#define FW_DRAW_TEXT     0xC166F4E0u   // void draw_text(void *canvas, const char *s, const int *xy, int grey, int mode)

#define FW_STORE_SCENE   0xC1570F60u   // int current_scene(void)  (= *(DP+684))
#define HELD_ADDR        0xC1501DF0u   // g_control_switch_state (u64): bit 19 = FUNC
#define TRACK_TYPES      0xC16ED768u   // int[15]: 1 = LOOP
#define CLIP_TABLE       0xC13DAF08u   // + track*4416 + scene*276 : [+0 START][+4 END (0 = empty clip)]
#define MAX_TRACKS       10
#define MAX_GUARD        3000          // stock: START <= file_frames - 3000
#define BEATS_PER_BAR    4             // v1: 4/4 assumed (time signature is per scene)
#define FRAMES_X10_BPM   28800000u     // 48000*60*10
#define READOUT_X        92
#define READOUT_Y        100
#define GREY             15
#define FLAG_TOL         16            // frames: a value this close to a line counts as on it

#define FN(ret,addr,args)   ((ret(*)args)(addr))

static inline u32 divu(u32 a, u32 b) { return FN(u32, FW_DIVU, (u32,u32))(a, b); }

static inline s32 dp296(void) { s32 v; __asm__ volatile("ldw .D2T1 *+B14(296), %0\n\tnop 4" : "=a"(v)); return v; }
static inline s32 dp300(void) { s32 v; __asm__ volatile("ldw .D2T1 *+B14(300), %0\n\tnop 4" : "=a"(v)); return v; }
static inline s32 dp304(void) { s32 v; __asm__ volatile("ldw .D2T1 *+B14(304), %0\n\tnop 4" : "=a"(v)); return v; }   // 1 right after B toggled the page to END
static inline void dp296_set(s32 v) { __asm__ volatile("stw .D2T1 %0, *+B14(296)" :: "a"(v)); }
static inline void dp1240_set60(void) { s32 t = 60; __asm__ volatile("stw .D2T1 %0, *+B14(1240)" :: "a"(t)); }

/* Beat grid. A loop only plays right at the tempo it was recorded at, so the WAVE defines the grid:
 * bars = whole bars the file holds at the project tempo, beat = file_frames / (bars*4). Falls back to the
 * project tempo when the file is not (within 2%) a whole number of bars (imported audio).
 * Line k (beats from frame 0) is at (k*f16)>>4, f16 = frames per beat * 16 (sub-frame precision, no drift). */
static u32 fpb16(u32 len) {
    u32 t = FN(s32, FW_TEMPO_X10, (void))();
    if (t < 300 || t > 3000) t = 1200;
    u32 tempo16 = divu(FRAMES_X10_BPM * 16u, t);               // project-tempo beat, x16
    if (len == 0 || len > (0xFFFFFFFFu >> 4)) return tempo16;
    u32 bar16 = tempo16 * BEATS_PER_BAR;
    u32 bars = divu((len << 4) + (bar16 >> 1), bar16);          // nearest whole bars
    if (bars == 0) return tempo16;
    u32 want = bars * (bar16 >> 4);                              // frames those bars would take at project tempo
    u32 diff = (want > len) ? want - len : len - want;
    if (diff * 50 > len) return tempo16;                         // more than 2% off: not a bar-aligned loop
    return divu((len << 4) + ((bars * BEATS_PER_BAR) >> 1), bars * BEATS_PER_BAR);
}
static u32 line_of(u32 k, u32 f) { return (k * f) >> 4; }
static u32 beat_floor(u32 p, u32 f) { return divu(p << 4, f); }
static u32 align_down(u32 k, u32 u) { return divu(k, u) * u; }

static void fmt_pos(char *o, s32 p, u32 len) {     // "B65.3": bar.beat (1-based) of the nearest beat line
    u32 f = fpb16(len);
    u32 q = (p < 0) ? 0 : (u32)p;
    u32 beats = divu((q << 4) + (f >> 1), f);
    u32 bar = divu(beats, BEATS_PER_BAR);
    u32 beat = beats - bar * BEATS_PER_BAR;
    bar += 1; beat += 1;
    char t[12]; int n = 0;
    *o++ = 'B';
    do { u32 d = divu(bar, 10); t[n++] = '0' + (bar - d * 10); bar = d; } while (bar && n < 10);
    while (n) *o++ = t[--n];
    *o++ = '.'; *o++ = '0' + beat; *o = 0;
}

void loopstart_draw(void *self, void *canvas) {
    FN(void, FW_DRAW_ORIG, (void*,void*))(self, canvas);
    if (dp300() != 1 || dp304() != 0) return;       // only on the START POS sub-page, and only while START (not END) is shown
    s32 v = ((s32*)self)[6];
    if (v == 0) return;                             // stock shows no START line yet
    char buf[16]; int xy[2] = { READOUT_X, READOUT_Y };
    fmt_pos(buf, v, FN(u32, FW_CLIP_LEN, (int))(dp296()));
    FN(void, FW_DRAW_TEXT, (void*,const char*,const int*,int,int))(canvas, buf, xy, GREY, 1);
}

/* ---------------- scene-wide macros (v0.2) ----------------
 * FUNC + knob A  = SHIFT : every OTHER loop clip in the current scene moves by the same number of beats as this clip.
 * FUNC + knob B  = SET ALL: every other loop clip's START = the same bar as this clip's START, on that clip's own grid;
 *                  clips too short for that bar are left alone.
 * Writes go through the stock store/apply sequence with DP+296 (the "current clip track") temporarily pointed at each track. */
static int func_held(void) { return (int)((*(volatile u32 *)HELD_ADDR >> 19) & 1u); }
static volatile s32 *clip_rec(int tr, int sc) { return (volatile s32 *)(CLIP_TABLE + (u32)tr * 4416u + (u32)sc * 276u); }
static int is_loop(int tr) { return *(volatile s32 *)(TRACK_TYPES + 4u * (u32)tr) == 1; }

static void commit_other(int tr, s32 v) {
    s32 save = dp296();
    dp296_set(tr);
    FN(void, FW_STORE, (int))(v);
    void *obj = FN(void*, FW_REFRESH, (int))(tr);
    FN(void, FW_APPLY, (void*,int,int))(obj, v, 1);
    FN(void*, FW_REFRESH, (int))(tr);
    dp296_set(save);
}

static u32 clip_len(int tr, s32 end) {
    u32 len = FN(u32, FW_CLIP_LEN, (int))(tr);
    if (len == 0 || len > 0x7FFFFFFFu) len = (u32)end + 1u;
    return len;
}

static void macro_shift(int cur, s32 dbeats) {
    s32 sc = FN(s32, FW_STORE_SCENE, (void))();
    for (int tr = 0; tr < MAX_TRACKS; tr++) {
        if (tr == cur || !is_loop(tr)) continue;
        volatile s32 *r = clip_rec(tr, sc);
        s32 end = r[1];
        if (end <= 0) continue;                       // empty clip slot
        u32 len = clip_len(tr, end);
        u32 f = fpb16(len);
        s32 old = r[0]; if (old < 0) old = 0;
        s32 nk = (s32)divu(((u32)old << 4) + (f >> 1), f) + dbeats;   // nearest line + shift
        if (nk < 0) nk = 0;
        s32 nv = (s32)line_of((u32)nk, f);
        s32 hi = (s32)len - MAX_GUARD; if (hi < 0) hi = 0;
        if (nv > hi) nv = (s32)line_of(beat_floor((u32)hi, f), f);    // last line that fits
        if (nv != old) commit_other(tr, nv);
    }
}

static void macro_setall(int cur, u32 bar_index) {
    s32 sc = FN(s32, FW_STORE_SCENE, (void))();
    for (int tr = 0; tr < MAX_TRACKS; tr++) {
        if (tr == cur || !is_loop(tr)) continue;
        volatile s32 *r = clip_rec(tr, sc);
        s32 end = r[1];
        if (end <= 0) continue;
        u32 len = clip_len(tr, end);
        u32 f = fpb16(len);
        s32 nv = (s32)line_of(bar_index * BEATS_PER_BAR, f);
        s32 hi = (s32)len - MAX_GUARD;
        if (nv > hi) continue;                        // clip too short for this bar: leave it
        s32 old = r[0];
        if (nv != old) commit_other(tr, nv);
    }
}

/* After the stock handler: if the pending START moved, put it on a beat line (a bigger turn = whole bars). */
void loopstart_enc(void *self, int enc, int delta) {
    s32 v0 = ((s32*)self)[6];
    FN(void, FW_ENC_ORIG, (void*,int,int))(self, enc, delta);
    s32 v1 = ((s32*)self)[6];
    int track = dp296();

    if (enc == 1 && func_held() && dp300() == 1 && dp304() == 0) {       // FUNC + knob B: SET ALL (this clip included) to one bar
        u32 len = FN(u32, FW_CLIP_LEN, (int))(track);
        u32 f = fpb16(len);
        u32 barfr = line_of(BEATS_PER_BAR, f);
        u32 cur = (u32)(v1 < 0 ? 0 : v1);
        u32 bi = divu(cur + (barfr >> 1), barfr);                      // nearest bar to this clip's START
        s32 hi = (s32)len - MAX_GUARD; if (hi < 0) hi = 0;
        s32 tc = (s32)line_of(bi * BEATS_PER_BAR, f);
        if (tc > hi) {                                                  // last whole bar that fits this clip
            bi = divu(beat_floor((u32)hi, f), BEATS_PER_BAR);
            tc = (s32)line_of(bi * BEATS_PER_BAR, f);
        }
        macro_setall(track, bi);
        if (tc != v1) {
            FN(void, FW_STORE, (int))(tc);
            ((s32*)self)[6] = tc;
            void *obj = FN(void*, FW_REFRESH, (int))(track);
            FN(void, FW_APPLY, (void*,int,int))(obj, tc, 1);
            obj = FN(void*, FW_REFRESH, (int))(track);
            FN(void, FW_PREVIEW, (void*,int,float))(obj, tc, 0.25f);
            dp1240_set60();
        }
        return;
    }
    if (v1 == v0 || dp300() != 1 || dp304() != 0) return;   // never touch END-mode edits

    u32 len = FN(u32, FW_CLIP_LEN, (int))(track);
    u32 f = fpb16(len);
    u32 barfr = line_of(BEATS_PER_BAR, f);
    s32 hi = (s32)len - MAX_GUARD;   // stock maximum
    if (hi < 0) hi = 0;
    int up = v1 > v0;
    u32 mag = up ? (u32)(v1 - v0) : (u32)(v0 - v1);
    u32 unit = (mag >= (barfr >> 1)) ? BEATS_PER_BAR : 1;           // lines per step, in beats
    u32 ustep = line_of(unit, f);
    u32 n = divu(mag + (ustep >> 1), ustep);
    if (n < 1) n = 1;

    u32 base = (u32)(v0 < 0 ? 0 : v0);
    u32 k = beat_floor(base, f);                    // last line at or before base
    int on = (base - line_of(k, f) <= FLAG_TOL);
    if (!on && line_of(k + 1, f) - base <= FLAG_TOL) { k += 1; on = 1; }
    u32 kt;
    if (up) {
        kt = align_down(k + 1 + (n - 1) * unit, unit);
        if (kt <= k) kt = k + unit;
    } else {
        u32 first = on ? (k ? k - 1 : 0) : k;        // first line strictly below base
        u32 back = (n - 1) * unit;
        kt = align_down((first > back) ? first - back : 0, unit);
    }
    s32 t = (s32)line_of(kt, f);
    if (t > hi) {                                   // back off to the last aligned line that fits
        u32 kh = (hi > 0) ? beat_floor((u32)hi, f) : 0;
        t = (s32)line_of(align_down(kh, unit), f);
    }
    if (t < 0) t = 0;

    if (t != v1) {
        FN(void, FW_STORE, (int))(t);               // same sequence as the stock handler
        ((s32*)self)[6] = t;
        void *obj = FN(void*, FW_REFRESH, (int))(track);
        FN(void, FW_APPLY, (void*,int,int))(obj, t, 1);
        obj = FN(void*, FW_REFRESH, (int))(track);
        FN(void, FW_PREVIEW, (void*,int,float))(obj, t, 0.25f);
        dp1240_set60();
    }
    if (func_held() && t != v0) {                   // FUNC + knob A: SHIFT every other loop clip by the same beats
        s32 d = t - v0;
        u32 ad = d < 0 ? (u32)(-d) : (u32)d;
        s32 nb = (s32)divu((ad << 4) + (f >> 1), f);
        macro_shift(track, d < 0 ? -nb : nb);
    }
}
