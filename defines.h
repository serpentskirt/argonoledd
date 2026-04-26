#ifndef DEFINES_H
#define DEFINES_H

#define _POSIX_C_SOURCE               200809L
#define ARRAY_SIZE(a)                 (sizeof(a) / sizeof((a)[0]))
#define I2C_DEV                       "/dev/i2c-1"
#define OLED_ADDR                     0x3C
#define OLED_WIDTH                    128
#define OLED_HEIGHT                   64
#define OLED_CONTRAST                 0x7F // 0x00–0xFF
#define OLED_MULTIPLEX                0x3F // 1/64 duty
#define OLED_CLOCK_DIV                0x80
#define OLED_PRECHARGE                0xF1
#define OLED_COM_PINS                 0x12
#define OLED_VCOM_DETECT              0x40
#define OLED_BYTE_ALL_ON              0xFF
#define OLED_CONTROL_COMMAND          0x00
#define OLED_CONTROL_DATA             0x40
#define OLED_CMD_DISPLAY_OFF          0xAE
#define OLED_CMD_DISPLAY_ON           0xAF
#define OLED_CMD_SET_MEMORY_MODE      0x20
#define OLED_MEMORY_MODE_HORIZONTAL   0x00
#define OLED_CMD_SET_PAGE_START       0xB0
#define OLED_CMD_COM_SCAN_DEC         0xC8
#define OLED_CMD_LOW_COLUMN_START     0x00
#define OLED_CMD_HIGH_COLUMN_START    0x10
#define OLED_CMD_SET_START_LINE       0x40
#define OLED_CMD_SET_CONTRAST         0x81
#define OLED_CMD_SEGMENT_REMAP        0xA1
#define OLED_CMD_NORMAL_DISPLAY       0xA6
#define OLED_CMD_SET_MULTIPLEX        0xA8
#define OLED_CMD_DISPLAY_RAM          0xA4
#define OLED_CMD_SET_DISPLAY_OFFSET   0xD3
#define OLED_DISPLAY_OFFSET_NONE      0x00
#define OLED_CMD_SET_CLOCK_DIV        0xD5
#define OLED_CMD_SET_PRECHARGE        0xD9
#define OLED_CMD_SET_COM_PINS         0xDA
#define OLED_CMD_SET_VCOM_DETECT      0xDB
#define OLED_CMD_SET_CHARGE_PUMP      0x8D
#define OLED_CHARGE_PUMP_ENABLE       0x14
#define OLED_CMD_SET_COLUMN_ADDR      0x21
#define OLED_CMD_SET_PAGE_ADDR        0x22
#define OLED_PAGE_HEIGHT              8
#define OLED_PAGE_COUNT               (OLED_HEIGHT / OLED_PAGE_HEIGHT)
#define OLED_CHAR_DEGREE              167
#define FB_SIZE                       (OLED_WIDTH * OLED_PAGE_COUNT)
#define CONFIG_MAX_ENTRIES            64
#define MS_PER_SEC                    1000
#define NS_PER_MS                     1000000L
#define NS_PER_SEC                    1000000000L
#define NUM_FONT_CHARS                256
#define FONT_PATH_FMT                 "res/font%dx%d.bin"
#define FONT_FALLBACK_PATH            "res/font8x6.bin"
#define RESOURCE_PATH_MAX             128
#define FONT_FALLBACK_WIDTH           6
#define FONT_FALLBACK_HEIGHT          8
#define FONT_MIN_CHARWD               6
#define FONT_ASPECT_NUM               8
#define FONT_ASPECT_DEN               6
#define CMD_VERB_BUF_SIZE             16
#define CMD_HEADER_BUF_SIZE           15
#define CMD_MAX_DETAILS               6
#define CMD_DETAIL_WIDTH              20
#define CMD_LINE_MAX                  256
#define DISP_IDLE_TIMEOUT_MS          60000
#define DISP_TICK_MAX_MS              1000
#define DISP_TICK_MIN_MS              1
#define DISP_FALLBACK_TIMEOUT_MS      1000
#define BITMAP_COPY_MAX               1024
#define STATS_MAX_CORES               4
#define STATS_MAX_STORAGES            4
#define STATS_MAX_DISKS               4
#define STATS_MAX_RAIDS               4
#define STATS_MAX_RAID_MEMBERS        16
#define STATS_MAX_IPS                 4
#define STATS_DEVNAME_LEN             32
#define STATS_FIELD_BUF_SIZE          32
#define STATS_TOKEN_BUF_SIZE          64
#define STATS_LINE_BUF_SIZE           256
#define STATS_LONG_LINE_BUF_SIZE      512
#define PROC_STAT_PATH                "/proc/stat"
#define PROC_MEMINFO_PATH             "/proc/meminfo"
#define PROC_MOUNTS_PATH              "/proc/mounts"
#define PROC_MDSTAT_PATH              "/proc/mdstat"
#define SYS_BLOCK_PATH                "/sys/block"
#define SYS_CPU_TEMP_PATH             "/sys/class/thermal/thermal_zone0/temp"
#define SMARTCTL_PATH                 "/usr/sbin/smartctl"
#define SMART_ATTR_AIRFLOW_TEMP       190 // airflow temperature (WD)
#define SMART_ATTR_HDD_TEMP           194 // temperature celsius
#define TEMP_MILLIDEGREE_DIVISOR      1000
#define TEMP_SENTINEL_MIN             999
#define TEMP_SENTINEL_MAX             (-999)
#define BYTES_PER_KB                  1024UL
#define SECTOR_BYTES                  512
#define DISKSTAT_FIELD_READ_SECTORS   2
#define DISKSTAT_FIELD_WRITE_SECTORS  6
#define LAYOUT_DATA_X                 54
#define LAYOUT_DATA_W                 (OLED_WIDTH - LAYOUT_DATA_X)
#define LAYOUT_SMALL_CHARWD           6
#define LAYOUT_LARGE_CHARWD           8
#define CPU_MAX_DISPLAY_CORES         4
#define CPU_BAR_HEIGHT                2
#define CPU_BAR_Y_OFFSET              12
#define CPU_ROW_HEIGHT                16
#define CPU_BAR_WIDTH                 (OLED_WIDTH - LAYOUT_DATA_X - 4)
#define RAM_PCT_Y                     8
#define RAM_OF_Y                      24
#define RAM_TOTAL_Y                   40
#define STORAGE_MAX_DISPLAY           3
#define STORAGE_ROW_Y_START           16
#define STORAGE_ROW_HEIGHT            16
#define STORAGE_PCT_X                 50
#define STORAGE_PCT_X_OFFSET          13
#define STORAGE_SIZE_X                77
#define STORAGE_NAME_MAX_CHARS        8
#define TEMP_BAR_MAX_HEIGHT           21
#define TEMP_BAR_X                    24
#define TEMP_BAR_Y                    20
#define TEMP_BAR_WIDTH                3
#define TEMP_ROW_Y_START              8
#define TEMP_ROW_HEIGHT               8
#define TEMP_LABEL_SHORT_MAX          3
#define TEMP_LABEL_BUF_SIZE           8
#define TEMP_LINE_BUF_SIZE            48
#define TEMP_SINGLE_VALUE_Y           24
#define TEMP_SECOND_LINE_XOFFSET      30
#define CLOCK_DATE_Y                  8
#define CLOCK_WEEKDAY_Y               24
#define CLOCK_TIME_Y                  40
#define BW_MAX_DISPLAY                3
#define BW_HEADER_Y                   0
#define BW_COLHEADER_Y                16
#define BW_ROW_Y_START                32
#define BW_ROW_HEIGHT                 16
#define BW_PCT_X                      50
#define BW_PCT_X_OFFSET               13
#define BW_SIZE_X                     77
#define BW_NAME_MAX_CHARS             8
#define RAID_TITLE_Y                  0
#define RAID_VALUE_Y                  8
#define RAID_STATE_Y                  8
#define RAID_RESYNC_Y                 16
#define RAID_ACTIVE_Y                 32
#define RAID_WORKING_Y                40
#define RAID_FAILED_Y                 48
#define RAID_SIZE_Y                   56
#define RAID_NO_RAID_Y                24
#define IP_IFNAME_Y                   0
#define IP_ADDR_Y                     16
#define IP_NO_IP_Y                    16
#define MSG_HEADER_Y                  0
#define MSG_DETAIL_Y_START            16
#define MSG_DETAIL_ROW_HEIGHT         8
#define SCREEN_SHORT_BUF_SIZE         16
#define SCREEN_VALUE_BUF_SIZE         32
#define SCREEN_LINE_BUF_SIZE          64
#define SCREEN_RAID_LINE_BUF_SIZE     75
#define SOCKET_LISTEN_BACKLOG         4
#define SOCKET_CLIENT_READ_TIMEOUT_MS 250
#define SOCKET_PERMS                  0660
#define SOCKET_GROUP                  "argonoledd"
#define LOG_IDENT                     "argonoledd"
#define LOG_OPTIONS                   (LOG_PID | LOG_CONS)
#define LOG_FACILITY                  LOG_DAEMON
#define DEFAULT_CONFIG                "/etc/argonoledd.conf"
#define DEFAULT_SCREENS               "clock cpu storage bandwidth raid ram temp ip"
#define DEFAULT_DURATION              3
#define MAX_SCREEN_LIST               16
#define SOCKET_READ_BUF               256
#define SOCKET_PATH                   "/run/argonoledd.sock"

_Static_assert(OLED_WIDTH > 0, "OLED_WIDTH must be > 0");
_Static_assert(OLED_HEIGHT > 0, "OLED_HEIGHT must be > 0");
_Static_assert(MS_PER_SEC > 0, "MS_PER_SEC must be > 0");
_Static_assert(NS_PER_MS > 0, "NS_PER_MS must be > 0");
_Static_assert(NS_PER_SEC > 0, "NS_PER_SEC must be > 0");
_Static_assert(OLED_PAGE_HEIGHT > 0, "OLED_PAGE_HEIGHT must be > 0");
_Static_assert(FONT_ASPECT_DEN > 0, "FONT_ASPECT_DEN must be > 0");
_Static_assert(BYTES_PER_KB > 0, "BYTES_PER_KB must be > 0");
_Static_assert(TEMP_MILLIDEGREE_DIVISOR > 0, "TEMP_MILLIDEGREE_DIVISOR must be > 0");

#endif
