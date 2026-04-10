'use strict';

const express             = require('express');
const http                = require('http');
const { WebSocketServer } = require('ws');
const fs                  = require('fs');
const path                = require('path');
const logic               = require('./logic');

const PORT        = 3000;
const DB_PATH     = path.join(__dirname, 'db.json');
const CLIENT_ROOT = path.join(__dirname, '..');

const app = express();
app.use(express.json());

// ---- State persistence ----

function loadState() {
    try {
        return logic.loadStateFromJson(fs.readFileSync(DB_PATH, 'utf8'));
    } catch (_) {
        return { on: false, color: '#ffffff' };
    }
}

function saveState(s) {
    fs.writeFileSync(DB_PATH, JSON.stringify(s, null, 2));
}

let state = loadState();

// ---- WebSocket broadcast ----

function broadcast() {
    logic.broadcast(state, wss.clients);
}

// ---- REST API ----

app.get('/api/state', (_req, res) => res.json(state));

app.post('/api/power', (req, res) => {
    const result = logic.handlePowerRequest(state, req.body);
    if (result.error) return res.status(result.status).json({ error: result.error });
    state = result.state;
    saveState(state);
    broadcast();
    res.json(state);
});

app.post('/api/color', (req, res) => {
    const result = logic.handleColorRequest(state, req.body);
    if (result.error) return res.status(result.status).json({ error: result.error });
    state = result.state;
    saveState(state);
    broadcast();
    res.json(state);
});

// Serve MorrayClient static files
app.use(express.static(CLIENT_ROOT));

// ---- HTTP + WebSocket server ----

const server = http.createServer(app);
const wss    = new WebSocketServer({ server, path: '/ws' });

wss.on('connection', (ws) => ws.send(JSON.stringify(state)));

server.listen(PORT, () => {
    console.log(`MorrayGlow test server → http://localhost:${PORT}`);
    console.log(`State file            → ${DB_PATH}`);
});
