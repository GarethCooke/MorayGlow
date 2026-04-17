'use strict';

const express             = require('express');
const http                = require('http');
const { WebSocketServer } = require('ws');
const fs                  = require('fs');
const path                = require('path');
const logic               = require('./logic');

const PORT        = 3000;
const DB_PATH     = path.join(__dirname, 'db.json');
const CLIENT_ROOT = path.join(__dirname, '../../MorayServer/data');

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

app.post('/api/mode', (req, res) => {
    if (typeof req.body.cycle !== 'boolean') return res.status(400).json({ error: 'cycle required' });
    state = { ...state, cycle: req.body.cycle };
    saveState(state);
    broadcast();
    res.json(state);
});

app.get('/api/deviceinfo', (_req, res) => res.json({
    id: 'morayglow-mock',
    name: 'MorayGlow',
    url: 'http://morayglow-mock.local',
}));

app.get('/api/networkdata', (_req, res) => res.json({
    scanning: false,
    networks: [
        { ssid: 'MockNetwork',    rssi: -55, secure: true  },
        { ssid: 'AnotherNetwork', rssi: -72, secure: false },
    ],
}));

app.post('/api/networkset', (req, res) => {
    if (!req.body.ssid) return res.status(400).json({ error: 'ssid required' });
    res.json({ ok: true });
});

app.get('/api/devices', (_req, res) => res.json({
    devices: [{ id: 'morayglow-mock', ip: '127.0.0.1', url: 'http://localhost:3000' }],
}));

// Serve firmware data/ static files
app.use(express.static(CLIENT_ROOT));

// ---- HTTP + WebSocket server ----

const server = http.createServer(app);
const wss    = new WebSocketServer({ server, path: '/ws' });

wss.on('connection', (ws) => ws.send(JSON.stringify(state)));

server.listen(PORT, () => {
    console.log(`MorayGlow test server → http://localhost:${PORT}`);
    console.log(`State file            → ${DB_PATH}`);
});
