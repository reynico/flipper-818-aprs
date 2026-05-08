#pragma once

#include <furi.h>
#include <stdint.h>

#define TXT_LEN 68
#define TXT_N 16
#define POS_LEN 16
#define CALL_LEN 10
#define APRS_PATH_LEN 9
#define CALL_N 16
#define HAM_N 8

#define MY_CALL "FL1PER"
#define MY_TOCALL "APZFLP"

#define CFG_DIR "/ext/apps_data/aprstx"
#define CFG_FILE "/ext/apps_data/aprstx/cfg2.bin"
#define CALLBOOK_DIR "/ext/ham"
#define CALLBOOK_FILE "/ext/ham/callbook.txt"
#define MY_CALLS_FILE "/ext/ham/my-callsigns.txt"

typedef struct
{
    uint8_t dst_ssid;
    uint8_t repeat_n;
    uint8_t ham_index;
    uint16_t leadin_ms;
    uint16_t preamble_ms;
    uint8_t dra_freq_index;
    char custom_freq[12];
    uint8_t dra_volume;
    uint8_t dra_squelch;

    char bulletin[TXT_N][TXT_LEN];
    char status[TXT_N][TXT_LEN];
    char message[TXT_N][TXT_LEN];
    char calls[CALL_N][CALL_LEN];
    char pos_name[TXT_N][TXT_LEN];
    char pos_lat[TXT_N][POS_LEN];
    char pos_lon[TXT_N][POS_LEN];

    uint8_t bulletin_used[TXT_N];
    uint8_t status_used[TXT_N];
    uint8_t message_used[TXT_N];
    uint8_t calls_used[CALL_N];
    uint8_t pos_used[TXT_N];

    uint8_t bulletin_n;
    uint8_t status_n;
    uint8_t message_n;
    uint8_t calls_n;
    uint8_t pos_n;
    uint8_t aprs_path_index;
    char aprs_path_edit[APRS_PATH_LEN];
    uint8_t debug_tx;
    uint8_t debug_rx;
    uint8_t rx_notify;
    uint8_t gps_enabled;
    uint16_t beacon_interval;
    char gps_comment[TXT_LEN];
} FlipperHamCfg;

enum
{
    FlipperHamViewMenu = 0,
    FlipperHamViewSend,
    FlipperHamViewSettings,
    FlipperHamViewBulletin,
    FlipperHamViewStatus,
    FlipperHamViewMessage,
    FlipperHamViewMessageEdit,
    FlipperHamViewPosition,
    FlipperHamViewSsid,
    FlipperHamViewCall,
    FlipperHamViewBook,
    FlipperHamViewC2,
    FlipperHamViewTextInput,
    FlipperHamViewPosEdit,
    FlipperHamViewPosAction,
    FlipperHamViewHam,
    FlipperHamViewHamTx,
    FlipperHamViewReadme,
    FlipperHamViewSplash,
    FlipperHamViewTxSettings,
    FlipperHamViewRxSettings,
    FlipperHamViewGpsSettings,
    FlipperHamViewGpsAction,
};

enum
{
    FlipperHamMenuIndexSend = 0,
    FlipperHamMenuIndexRx,
    FlipperHamMenuIndexSettings,
    FlipperHamMenuIndexReadme,
};

enum
{
    FlipperHamSendIndexMessage = 0,
    FlipperHamSendIndexPosition,
    FlipperHamSendIndexStatus,
    FlipperHamSendIndexBulletin,
};

enum
{
    FlipperHamSettingsIndexVhfFreq = 0,
    FlipperHamSettingsIndexCustomFreq,
    FlipperHamSettingsIndexMyCallsign,
    FlipperHamSettingsIndexDestinations,
    FlipperHamSettingsIndexLocations,
    FlipperHamSettingsIndexAprsPath,
    FlipperHamSettingsIndexCustomPath,
    FlipperHamSettingsIndexTxSettings,
    FlipperHamSettingsIndexRxSettings,
    FlipperHamSettingsIndexGpsSettings,
};

enum
{
    FlipperHamTxSettingsIndexRepeat = 0,
    FlipperHamTxSettingsIndexLeadin,
    FlipperHamTxSettingsIndexPreamble,
    FlipperHamTxSettingsIndexDebugTx,
};

enum
{
    FlipperHamRxSettingsIndexVolume = 0,
    FlipperHamRxSettingsIndexSquelch,
    FlipperHamRxSettingsIndexSoundVibro,
    FlipperHamRxSettingsIndexDebugRx,
};

enum
{
    FlipperHamBulletinIndexAdd = 0,
    FlipperHamBulletinIndexBase = 100,
};

enum
{
    FlipperHamStatusIndexAdd = 0,
    FlipperHamStatusIndexBase = 200,
};

enum
{
    FlipperHamCallIndexAdd = 0,
    FlipperHamCallIndexBase = 300,
};

enum
{
    FlipperHamMessageIndexAdd = 0,
    FlipperHamMessageIndexBase = 400,
};

enum
{
    FlipperHamPositionIndexAdd = 0,
    FlipperHamPositionIndexGps = 1,
    FlipperHamPositionIndexBase = 450,
};

enum
{
    FlipperHamGpsActionSendOnce = 0,
    FlipperHamGpsActionBeacon,
    FlipperHamGpsActionComment,
    FlipperHamGpsActionClearComment,
};

enum
{
    FlipperHamPosEditIndexName = 0,
    FlipperHamPosEditIndexLat,
    FlipperHamPosEditIndexLon,
    FlipperHamPosEditIndexDelete,
};

enum
{
    FlipperHamBookIndexAdd = 0,
    FlipperHamBookIndexBase = 500,
};

enum
{
    FlipperHamC2IndexEdit = 0,
    FlipperHamC2IndexDelete,
    FlipperHamC2IndexCopy,
};

int32_t flipperham_app(void *p);
