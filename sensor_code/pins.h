// vibrate port and pin parameters, matching Atmel SAM R21 board
#ifdef BOARD_SAMR21_XPRO
#define VBR_PORT    0   // Port+Pin A13
#define VBR_PIN     13
#else
// Port and pin parameters, matching Phytec phyWAVE
//#ifdef BOARD_PBA_D_01_KW2X
#define VBR_PORT    0   // Port+Pin A1
#define VBR_PIN     1
#define BTN_PORT    4   // Port+Pin E2
#define BTN_PIN     2
#endif
