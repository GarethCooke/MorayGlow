#pragma once

// Pushes current LED state to all connected WebSocket clients.
// Called from main.cpp and mqtt.cpp on any state change.
void broadcastState();

void webserverSetup();
void webserverLoop();
