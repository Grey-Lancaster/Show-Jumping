// Show jumping sounds, all synthesised (no recordings): hoofbeats, a knocked rail, the start bell, applause.
#pragma once
#include <Arduino.h>
void audioMix(int16_t *out, int n);     // n mono samples at 16 kHz: the effects
void audioHoof(float loud);             // one hoofbeat (0..1)
void audioKnock();                      // a pole hits the ground
void audioBell();                       // the judge's start bell
void audioApplause(float secs);         // the crowd claps for a while
void audioEngine(bool on);              // the arena tractor putt-putting
#ifdef BOARD_CYD28
void audioBubbles44(int16_t *out, int n);   // CYD: the 44.1 kHz ambience stream (silent here; the HAL expects it)
#endif
