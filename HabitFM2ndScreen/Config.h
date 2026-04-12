#ifndef CONFIG_H
#define CONFIG_H

#include <TFT_eSPI.h>
#include <AnimatedGIF.h>
#include <RTClib.h>

// --- COLORS ---
#define BG_BROWN      0x8A22 
#define RING_LEAF     0x4520 
#define TEXT_LEAF     0x57E0 
#define RING_SAGE     0xB7E0 
#define TEXT_PARCH    0xFFFF 

// --- UI GEOMETRY ---
#define CIRCLE_X      120    
#define CIRCLE_Y      120    
#define BORDER_X      230    
#define PANEL_CENTER  275    
#define BRD_THICK     4      

// --- ENUMS ---
enum AppMode { DASHBOARD, VICTORY_WAIT, VICTORY_PLAY };
enum DrawingType { DRAW_FIRE, DRAW_LEADER, DRAW_SQUAD, DRAW_VICTORY };

// --- GLOBAL DECLARATIONS (extern tells other files these exist elsewhere) ---
extern TFT_eSPI tft;
extern RTC_DS3231 rtc;
extern AnimatedGIF gifVictory, gifFire, gifLeader, gifSquad;

#endif