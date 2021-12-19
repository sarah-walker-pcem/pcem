#define SCAT_DMA_WAIT_STATE_CONTROL 0x01
#define SCAT_VERSION                0x40
#define SCAT_CLOCK_CONTROL          0x41
#define SCAT_PERIPHERAL_CONTROL     0x44
#define SCAT_MISCELLANEOUS_STATUS   0x45
#define SCAT_POWER_MANAGEMENT       0x46
#define SCAT_ROM_ENABLE             0x48
#define SCAT_RAM_WRITE_PROTECT      0x49
#define SCAT_SHADOW_RAM_ENABLE_1    0x4A
#define SCAT_SHADOW_RAM_ENABLE_2    0x4B
#define SCAT_SHADOW_RAM_ENABLE_3    0x4C
#define SCAT_DRAM_CONFIGURATION     0x4D
#define SCAT_EXTENDED_BOUNDARY      0x4E
#define SCAT_EMS_CONTROL            0x4F

#define SCATSX_LAPTOP_FEATURES          0x60
#define SCATSX_FAST_VIDEO_CONTROL       0x61
#define SCATSX_FAST_VIDEORAM_ENABLE     0x62
#define SCATSX_HIGH_PERFORMANCE_REFRESH 0x63
#define SCATSX_CAS_TIMING_FOR_DMA       0x64

typedef struct scat_t
{
        uint8_t regs_2x8;
        uint8_t regs_2x9;
} scat_t;

void scat_init();
void scatsx_init();
