/* smpltrek-loopstart v0.3: loop-clip START in beats/bars, scene-wide shift / set-all, a clip list page, "modulo" looping, and release of the
 * scene hold when leaving the clip EDIT page.   SmplTrek SYSTEM 3.2.174 ONLY.
 * Appended-section payload linked at 0xC3F00000 (link.ld). Hooks = three words of the AppLoopTrackView vtable (vptr 0xC16C5978):
 * close (slot 1, word 0xC16C597C), handleEncoder (+28, word 0xC16C5994) and draw (+56, word 0xC16C59B0).
 *
 * Facts established in the emulator on the stock firmware (more in docs/knowledge-base.md):
 *   - the START POS sub-page is a MODE of AppLoopTrackView: *(DP+300)==1 while it is open (B pressed), 0 after OK/CLR
 *   - *(DP+296) = track index of the clip being edited
 *   - encoder A (index 0) edits the PENDING start in view->+24 (frames @48k); OK commits it
 *   - stock click size = file_frames/128; stock max = file_frames - 3000; file frames = 0xC15C56E4(track)
 *   - after each change the stock handler does: refresh(0xC155F36C), apply(0xC1569680 track,val,1), refresh, preview(0xC1568970 track,val,0.25f)
 * Rules: no libc (divide via firmware __c6xabi_divu; libgcc only for the compiler's register save/restore helpers), B14 is never changed;
 * a few data-page words are written with single stw instructions.
 */
typedef unsigned int u32;
typedef int s32;
typedef unsigned char u8;

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
#define FW_PUSH_PAGE     0xC15F69C8u   // push_page(owner, child): owner+12 = the top controller; top.child(+8)=child; child.parent(+4)=top; child.vptr[0](child). We do the same by hand (the owner object is not known).
#define FW_CLOSE_PAGE    0xC16953C0u   // void close_page(void *page)                calls the page's close (slot 1) and detaches it from its parent
#define FW_BASE_CTOR     0xC15D3B24u   // void *ApplicationController base constructor(this)
#define FW_PAGE_TITLE    0xC15D3240u   // void draw_page_title(canvas, const char *title, int style)
#define SYSINFO_VTABLE   0xC16C5330u   // AppSystemInfo vtable: 2 header words, then 18 slots (vptr = +8)
#define HOME_CTRL        0xC15037F0u   // the home grid controller object: +24 track cursor, +28 scene cursor
#define ENGINE_OBJ       0xC14A63B4u   // per loop track engine object (224 B each): +0 START, +8 playback position in file frames (read-ahead ~4096 frames)
#define SEQ_LEN_ADDR     0xC143467Cu   // track 0 SEQ-tab LEN (int); +0x4D4 per track. The NOTE length in ticks (36 = a quarter, 9 = 1/16) is 0x10 lower. [EMU, docs/knowledge-base.md]
#define SEQ_STRIDE       0x4D4u
#define POS_LATENCY      4096          // the position field runs this far ahead of what is audible (a parked clip sits at START + 4096)
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
static inline void dp680_set1(void) { s32 t = 1; __asm__ volatile("stw .D2T1 %0, *+B14(680)" :: "a"(t)); }   // the scene auto-advance flag (0 = hold)
static inline void dp504_set(s32 v) { __asm__ volatile("stw .D2T1 %0, *+B14(504)" :: "a"(v)); }   // mirror of the home grid's track cursor
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

/* ---------------- clip list page: object and vtable ----------------
 * A page of our own: object + vtable live in OUR section. The vtable is a copy of AppSystemInfo's (most slots are shared do-nothing stubs)
 * with three slots replaced: 4 buttons, 14 draw content, 15 draw title. Opened with the firmware's own push_page from a chord (FUNC + knob D)
 * on the clip EDIT page; CLR (or the REC key, like AppSystemInfo) closes it with the firmware's close_page. */
static u32 hello_vt[24]  = { 0xFFFFFFFFu };     // non-zero initialiser keeps these in the loaded .data, not .bss
static u32 hello_obj[96] = { 0xFFFFFFFFu };
static u32 hello_state   = 1;                   // 1 = vtable not built yet, 0 = built
static u32 hello_frames  = 1;

static void hello_text(void *canvas, const char *s, int x, int y) {
    int xy[2] = { x, y };
    FN(void, FW_DRAW_TEXT, (void*,const char*,const int*,int,int))(canvas, s, xy, GREY, 1);
}
static void clips_draw(void *self, void *canvas);
static void clips_title(void *self, void *canvas);
static int  clips_buttons(void *self, const u8 *msg);
static int  clips_enc(void *self, int enc, int delta);
#define TRK_STATE(tr) (*(volatile u32 *)(0xC14EB798u + 72u * (u32)(tr) + 8u))   // per loop track play state: 2 idle, 32 playing, 64 one-shot finished [EMU]
static u32 clips_play = 0xFFFFFFFFu;   // set by clips_draw: some listed track is playing or finished (= transport running)
static u32 clips_sel = 0xFFFFFFFFu;   // selected track index (validated on use); non-zero initial value keeps it in the loaded data
static int hello_buttons(void *self, const u8 *msg) {   // msg: +4 64-bit key mask, +12 event code (as AppSystemInfo reads them)
    u32 lo = *(const u32 *)(msg + 4), hi = *(const u32 *)(msg + 8);
    u32 code = (u32)msg[12] | ((u32)msg[13] << 8);
    if (code == 1 && hi == 0 && (lo & (1u << 30 | 1u << 26))) {   // CLR (bit 30) or REC (bit 26): close
        FN(void, FW_CLOSE_PAGE, (void*))(self);
        return 1;
    }
    return 0;
}
static void hello_open(void *parent) {
    if (hello_state) {
        for (int i = 0; i < 20; i++) hello_vt[i] = ((const u32 *)SYSINFO_VTABLE)[i];   // header words + 18 slots
        hello_vt[2 + 4]  = (u32)(void *)clips_buttons;
        hello_vt[2 + 7]  = (u32)(void *)clips_enc;
        hello_vt[2 + 14] = (u32)(void *)clips_draw;
        hello_vt[2 + 15] = (u32)(void *)clips_title;
        hello_state = 0;
    }
    for (int i = 0; i < 96; i++) hello_obj[i] = 0;
    clips_sel = (u32)dp296();                                                       // start the selection on the clip you came from
    FN(void*, FW_BASE_CTOR, (void*))(hello_obj);
    hello_obj[0] = (u32)(void *)&hello_vt[2];                                       // vptr = first slot
    ((u32 *)parent)[2] = (u32)(void *)hello_obj;                                    // what push_page does: parent.child = us,
    hello_obj[1] = (u32)parent;                                                     //                    us.parent = parent,
    ((void (*)(void *))(((u32 *)hello_obj[0])[0]))(hello_obj);                      //                    then call our open (slot 0)
}

/* ---------------- scene-wide macros (v0.2) ----------------
 * FUNC + knob A  = SHIFT : every OTHER loop clip in the current scene moves by the same number of beats as this clip (a clip too short for the move rotates).
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

/* Loop length ("MODLOOP"): the live SEQ record of a clip (track x scene) = the store that playback and SAVE use [EMU]. Byte +2 = LEN, BYTE +4 = NOTE code (byte +5 holds something else, 0xFF in some projects: never touch it). */
#define SEQ_REC(tr, sc) ((volatile u8 *)(0xC143A6E8u + (u32)(tr) * 140u + (u32)(sc) * 1400u))
/* After a clip's START moved: loop what is left of the clip (clip length - START, in beats). START at beat 1 restores the whole clip.
 * LEN is 1..128, so: quarter notes up to 128 beats, half notes to 256, whole bars beyond (a remainder that does not fit is rounded UP). */
static void set_loop(int tr, s32 start) {
    s32 sc = FN(s32, FW_STORE_SCENE, (void))();
    s32 end = clip_rec(tr, sc)[1];
    if (end <= 0) return;
    u32 len = clip_len(tr, end), f = fpb16(len);
    u32 nbeats = divu((len << 4) + (f >> 1), f);
    u32 sb = divu(((u32)start << 4) + (f >> 1), f);
    u32 rem = (sb < nbeats) ? nbeats - sb : 1;
    u32 code = 3;                                    // 1/4
    if (rem > 256) { rem = (rem + 3) >> 2; code = 0; if (rem > 128) rem = 128; }   // 1/1
    else if (rem > 128) { rem = (rem + 1) >> 1; code = 1; }                       // 1/2
    if (rem < 1) rem = 1;
    volatile u8 *q = SEQ_REC(tr, sc);
    q[2] = (u8)rem;
    q[4] = (u8)code;
}

static void macro_shift(int cur, s32 dbeats) {
    /* Every other loop clip moves by the same number of beats; a clip too short for the move ROTATES: its new start is (nearest beat + shift) modulo its own length in
     * beats, so a 1-bar loop shifted by 6 beats lands on beat 2, as if it kept looping. The edited clip itself is limited by the stock handler. */
    s32 sc = FN(s32, FW_STORE_SCENE, (void))();
    for (int tr = 0; tr < MAX_TRACKS; tr++) {
        if (tr == cur || !is_loop(tr)) continue;
        volatile s32 *r = clip_rec(tr, sc);
        s32 end = r[1];
        if (end <= 0) continue;                       // empty clip slot
        u32 len = clip_len(tr, end);
        u32 f = fpb16(len);
        u32 nbeats = divu((len << 4) + (f >> 1), f);  // clip length in beats
        if (nbeats < 1) nbeats = 1;
        s32 old = r[0]; if (old < 0) old = 0;
        s32 nk = (s32)divu(((u32)old << 4) + (f >> 1), f) + dbeats;
        if (nk >= 0) nk = (s32)((u32)nk - divu((u32)nk, nbeats) * nbeats);                 // rotate into [0, nbeats)
        else         nk = nk + (s32)(divu((u32)(-nk) + nbeats - 1, nbeats) * nbeats);
        s32 nv = (s32)line_of((u32)nk, f);
        s32 hi = (s32)len - MAX_GUARD; if (hi < 0) hi = 0;
        if (nv > hi) nv = (s32)line_of(beat_floor((u32)hi, f), f);    // safety: never past the stock maximum
        if (nv != old) commit_other(tr, nv);
        set_loop(tr, nv);
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
        u32 nbeats = divu((len << 4) + (f >> 1), f), k = bar_index * BEATS_PER_BAR;
        if (nbeats < 1) nbeats = 1;
        k -= divu(k, nbeats) * nbeats;                // past the end of a shorter clip: wrap (modulo its own length)
        s32 nv = (s32)line_of(k, f);
        s32 hi = (s32)len - MAX_GUARD; if (hi < 0) hi = 0;
        if (nv > hi) nv = (s32)line_of(beat_floor((u32)hi, f), f);
        s32 old = r[0];
        if (nv != old) commit_other(tr, nv);
        set_loop(tr, nv);
    }
}

/* ---------------- clip list page (v0.3 experiment) ----------------
 * Read-only list of the loop clips in the current scene: track, START as bar.beat on that clip's own grid, and the clip's length in bars.
 * Reads the live clip table every frame, so it follows any edit. Opened by FUNC + knob D on the clip EDIT page; CLR closes it. */
static char *put_dec(char *o, u32 n) {
    char t[12]; int k = 0;
    do { u32 d = divu(n, 10); t[k++] = (char)('0' + (n - d * 10)); n = d; } while (n && k < 11);
    while (k) *o++ = t[--k];
    return o;
}
static char *put_str(char *o, const char *s) { while (*s) *o++ = *s++; return o; }

static int clips_pick(s32 sc, int dir) {            // the selected track if it is still a listed clip (else the first one), moved dir rows with wrap; -1 = none
    signed char v[MAX_TRACKS]; int n = 0, i = 0;
    for (int tr = 0; tr < MAX_TRACKS; tr++)
        if (is_loop(tr) && clip_rec(tr, sc)[1] > 0) { if (tr == (int)clips_sel) i = n; v[n++] = (signed char)tr; }
    if (n == 0) return -1;
    i += dir;
    if (i < 0) i = n - 1;
    if (i >= n) i = 0;
    return v[i];
}
static void clips_move(s32 sc, int dir) {
    int tr = clips_pick(sc, dir);
    if (tr >= 0) clips_sel = (u32)tr;
}

static int clips_buttons(void *self, const u8 *msg) {   // UP/DOWN move the selection, OK jumps to that clip's page, CLR/REC leave
    u32 lo = *(const u32 *)(msg + 4), hi = *(const u32 *)(msg + 8);
    u32 code = (u32)msg[12] | ((u32)msg[13] << 8);
    if (code != 1 || hi != 0) return 0;
    s32 sc = FN(s32, FW_STORE_SCENE, (void))();
    if (lo & (1u << 25)) { clips_move(sc, +1); return 1; }          // DOWN
    if (lo & (1u << 17)) { clips_move(sc, -1); return 1; }          // UP
    if (lo & (1u << 23)) {                                           // OK: make this clip the one the edit page shows, then go back to it
        int tr = clips_pick(sc, 0);
        if (tr >= 0) {                                   // the home grid's track cursor (HOME+24), its DP mirror and the edit page's track
            *(volatile s32 *)(HOME_CTRL + 24) = tr;
            dp504_set(tr);
            dp296_set(tr);
        }
        FN(void, FW_CLOSE_PAGE, (void*))(self);
        return 1;
    }
    if (lo & (1u << 30 | 1u << 26)) { FN(void, FW_CLOSE_PAGE, (void*))(self); return 1; }   // CLR or REC
    return 0;
}

static int clips_enc(void *self, int enc, int delta) {  // knob A: START of the selected clip +/- beats (FUNC held: all other loop clips follow); VALUE knob: move selection
    (void)self;
    s32 sc = FN(s32, FW_STORE_SCENE, (void))();
    if (enc == 4) { clips_move(sc, delta > 0 ? +1 : -1); return 1; }
    if (enc != 0) return 0;
    int tr = clips_pick(sc, 0);
    if (tr < 0) return 0;
    volatile s32 *r = clip_rec(tr, sc);
    s32 end = r[1];
    u32 len = clip_len(tr, end), f = fpb16(len);
    s32 old = r[0]; if (old < 0) old = 0;
    s32 k = (s32)divu(((u32)old << 4) + (f >> 1), f);               // nearest beat line to the current START
    s32 nk = k + delta; if (nk < 0) nk = 0;
    s32 nv = (s32)line_of((u32)nk, f);
    s32 hi = (s32)len - MAX_GUARD; if (hi < 0) hi = 0;
    if (nv > hi) nv = (s32)line_of(beat_floor((u32)hi, f), f);      // last line that fits
    if (nv == old) return 1;
    commit_other(tr, nv);
    set_loop(tr, nv);
    if (func_held()) {
        s32 d = nv - old; u32 ad = d < 0 ? (u32)(-d) : (u32)d;
        s32 nb = (s32)divu((ad << 4) + (f >> 1), f);
        macro_shift(tr, d < 0 ? -nb : nb);
    }
    return 1;
}

static void fmt_posf(char *o, u32 p, u32 f) {            // "B3.2": bar.beat the position p is IN (floor, not nearest)
    u32 beats = divu(p << 4, f);
    u32 bar = divu(beats, BEATS_PER_BAR), beat = beats - bar * BEATS_PER_BAR;
    *o++ = 'B'; o = put_dec(o, bar + 1); *o++ = '.'; *o++ = (char)('0' + beat + 1); *o = 0;
}
static char *put_pad(char *o, const char *s, int width) {
    int n = 0; while (*s) { *o++ = *s++; n++; }
    do { *o++ = ' '; } while (++n < width);   // always at least one space: columns never touch
    return o;
}

static void clips_title(void *self, void *canvas) {
    (void)self;
    char b[24]; char *o = put_str(b, "SCENE ");
    o = put_dec(o, (u32)FN(s32, FW_STORE_SCENE, (void))() + 1u);      // the home grid's scene cursor (it follows the song while playing)
    o = put_str(o, clips_play == 1u ? " PLAY" : " STOP");
    *o = 0;
    FN(void, FW_PAGE_TITLE, (void*,const char*,int))(canvas, b, 0);
}

static void clips_draw(void *self, void *canvas) {
    (void)self;
    s32 sc = FN(s32, FW_STORE_SCENE, (void))();
    hello_text(canvas, "T STRT NOW   LN  SEQ", 6, 16);
    int cur = clips_pick(sc, 0);
    int row = 0;
    u32 play = 0;
    for (int tr = 0; tr < MAX_TRACKS; tr++) {
        if (!is_loop(tr)) continue;
        volatile s32 *r = clip_rec(tr, sc);
        s32 end = r[1];
        if (end <= 0) continue;                         // empty slot
        u32 len = clip_len(tr, end);
        u32 f = fpb16(len);
        char b[48]; char *o = b, tmp[16];
        o = put_dec(o, (u32)tr + 1u);
        *o++ = ' ';
        fmt_pos(tmp, r[0], len);                        // START, nearest beat: "B65.3"
        o = put_pad(o, tmp + 1, 5);                     // shown without the leading "B": the header names the column
        u32 rec = *(volatile u32 *)(0xC14EB798u + 72u * (u32)tr + 36u);      // the track record's pointer to its playback record (not always in track order)
        if (rec < 0xC14A0000u || rec > 0xC14C0000u) continue;
        u32 ea = rec + 68u, eb = ea + 112u;   // two banks per track (scenes alternate between them): the live one has an END, and the further-along one if both do
        volatile u32 *oa = (volatile u32 *)ea, *ob = (volatile u32 *)eb;
        u32 sa = TRK_STATE(tr), sb = TRK_STATE(tr + 10);                    // play state of bank A / bank B (the second bank's entry is 10 further on)
        volatile u32 *eo = (sb >= 32u && sa < 32u) ? ob : oa;              // the bank that is running
        u32 gs = (eo == ob) ? sb : sa;
        u32 pos = eo[2];                                                    // live playback position in the clip's file
        u32 st0 = eo[0];                                                    // the engine's copy of START
        if (gs >= 32u) play = 1;
        if (gs != 32u || pos <= st0 + POS_LATENCY + 64u) { tmp[0] = '-'; tmp[1] = '-'; tmp[2] = 0; }   // parked at START + read-ahead: stopped or finished
        else fmt_posf(tmp, pos - POS_LATENCY, f);
        u32 beats = divu((len << 4) + (f >> 1), f);
        o = put_pad(o, tmp[0] == 'B' ? tmp + 1 : tmp, 6);
        *put_dec(tmp, divu(beats + (BEATS_PER_BAR >> 1), BEATS_PER_BAR)) = 0;
        o = put_pad(o, tmp, 4);                         // clip (file) length in bars
        {   // loop length (SEQ LEN x NOTE) from the clip's record, in bars (a quarter note = 72 units, a bar = 288); one decimal below 10 bars,
            // "+" for a fraction above (screen width)
            static const unsigned short nv72[11] = { 288, 144, 108, 72, 54, 48, 36, 27, 24, 18, 9 };
            volatile u8 *q = SEQ_REC(tr, sc);
            u32 code = q[4]; if (code > 10u) code = 3u;
            u32 tenths = divu((u32)nv72[code] * (u32)q[2] * 10u + 144u, 288u);
            u32 whole = divu(tenths, 10u), frac = tenths - whole * 10u;
            o = put_dec(o, whole);
            if (frac) { if (whole < 10u) { *o++ = '.'; *o++ = (char)('0' + frac); } else *o++ = '+'; }
        }
        *o = 0;
        hello_text(canvas, b, 6, 27 + 9 * row);
        if (tr == cur) hello_text(canvas, ">", 0, 27 + 9 * row);
        row++;
    }
    clips_play = play;                                  // the title (drawn next) shows PLAY or STOP
    if (row == 0) hello_text(canvas, "NO LOOP CLIPS", 20, 56);
    else if (row <= 7) hello_text(canvas, "A=START  OK=GO", 12, 100);
}

/* After the stock handler: if the pending START moved, put it on a beat line (a bigger turn = whole bars). */
void loopstart_enc(void *self, int enc, int delta) {
    if (enc == 3 && func_held()) { hello_open(self); return; }      // FUNC + knob D: open the hello page 
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
        set_loop(track, tc);
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
    if (t != v0) set_loop(track, t);                // every START change sets that clip's loop to what remains
    if (func_held() && t != v0) {                   // FUNC + knob A: SHIFT every other loop clip by the same beats
        s32 d = t - v0;
        u32 ad = d < 0 ? (u32)(-d) : (u32)d;
        s32 nb = (s32)divu((ad << 4) + (f >> 1), f);
        macro_shift(track, d < 0 ? -nb : nb);
    }
}

/* Leaving the clip EDIT page (vtable slot 1): the stock page latches the scene HOLD flag (DP+680 = 0) when you enter it during playback and never
 * releases it until STOP. Release it here so the timeline carries on from the home screen. The hold still applies while the EDIT page is open. */
void loopstart_leave(void *self) {
    FN(void, 0xC15608B4u, (void*))(self);
    dp680_set1();
}
