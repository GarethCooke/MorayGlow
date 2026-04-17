#pragma once
#include <ESPAsyncWebServer.h>

// Pushes current LED state to all connected WebSocket clients.
void broadcastState();

void webserverSetup(AsyncWebServer& server);
void webserverLoop();
