#pragma once
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/toolbox/level_duration.h>

#define RF_SIG_BUF       512U
#define RF_WORK_BUF      256U
#define RF_MIN_EDGES      16U
#define RF_SILENCE_US  25000U
#define RF_TIMEOUT_MS   9000U
#define RF_FREQ_DEF  433920000UL

typedef enum { RF_IDLE, RF_CAPTURING, RF_JAMMING, RF_REPLAYING } RfOp;
typedef enum { MOD_MENU, MOD_ROLLJAM, MOD_ROLLLAB, MOD_JAMMER }  ActiveMod;

typedef struct {
    volatile uint32_t buf[RF_SIG_BUF];
    volatile size_t   count;
    volatile bool     ready;
} RfSig;

// ---- RollJam ----
typedef enum {
    RJ_IDLE,
    RJ_P1_JAM, RJ_P1_RX,   // Phase 1: interlaced jam/rx (non-blocking)
    RJ_CAPTURED,
    RJ_P2_JAM, RJ_P2_RX,   // Phase 2: interlaced jam/rx (non-blocking)
    RJ_REPLAY,
    RJ_DONE
} RjPhase;
typedef struct {
    RjPhase  phase;
    RfSig    sig_a, sig_b;
    volatile size_t tx_pos;
    uint32_t phase_start_ms; // start of current P1/P2 attempt (for timeout)
    uint32_t step_start_ms;  // start of current jam or rx window
    char     status[64];
} ModRollJam;

// ---- RollLab ----
typedef enum {
    RL_MENU, RL_CAPTURING, RL_ANALYZE_VIEW,
    RL_REPLAY_READY, RL_REPLAYING,
    RL_ADVANCE, RL_PROBE_READY, RL_PROBING, RL_RESULT
} RlPhase;
typedef enum { RL_ANALYZE, RL_REPLAY, RL_ROLLBACK, RL_SYNCWIN } RlMode;
typedef struct { uint32_t edges, min_us, max_us, avg_us, total_ms; bool preamble_ok, looks_ook; } RlAnalysis;
typedef struct {
    RlPhase  phase;
    RlMode   mode;
    RfSig    ref_sig;
    volatile uint32_t work_buf[RF_WORK_BUF];
    volatile size_t   work_count;
    volatile bool     work_ready;
    RlAnalysis analysis;
    uint8_t  menu_idx, advance_count, advance_target;
    volatile size_t tx_pos;
    uint32_t cap_start_ms;
    uint32_t work_delay_until; // tick-basierter Delay für RL_ADVANCE (ersetzt furi_delay_ms)
    char     status[64];
} ModRollLab;

// ---- Jammer ----
typedef enum { JM_IDLE, JM_JAMMING } JmPhase;
#define JM_FREQ_COUNT 4
typedef struct {
    JmPhase phase;
    uint8_t freq_idx;
    char    status[48];
} ModJammer;

// ---- Main App ----
typedef struct {
    ActiveMod           active;
    uint8_t             menu_idx;
    const SubGhzDevice* device;     // aktiv genutztes Gerät
    const SubGhzDevice* device_int; // int CC1101, immer gesetzt nach init
    const SubGhzDevice* device_ext; // ext CC1101, NULL wenn nicht verbunden
    bool                dev_is_ext; // set at init, safe to read from any thread
    uint32_t            frequency;
    RfOp                rf_op;
    volatile bool       abort;

    ModRollJam  rj;
    ModRollLab  rl;
    ModJammer   jm;

    FuriMessageQueue* eq;
    ViewPort*         vp;
    Gui*              gui;
    NotificationApp*  notif;
} RollForgeApp;
