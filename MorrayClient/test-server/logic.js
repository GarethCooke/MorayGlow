'use strict';

function isValidHexColor(color) {
    return typeof color === 'string' && /^#[0-9a-fA-F]{6}$/.test(color);
}

// Parses a JSON string into state. Returns default state on failure.
function loadStateFromJson(jsonString) {
    try {
        const parsed = JSON.parse(jsonString);
        if (typeof parsed.on !== 'boolean' || typeof parsed.color !== 'string') {
            return { on: false, color: '#ffffff' };
        }
        return parsed;
    } catch (_) {
        return { on: false, color: '#ffffff' };
    }
}

// Returns { state, status } or { state, status, error }.
function handlePowerRequest(state, body) {
    if (typeof body.on !== 'boolean') {
        return { state, status: 400, error: "missing 'on' boolean" };
    }
    return { state: { ...state, on: body.on }, status: 200 };
}

// Returns { state, status } or { state, status, error }.
function handleColorRequest(state, body) {
    if (!isValidHexColor(body.color)) {
        return { state, status: 400, error: "invalid 'color' — must be #rrggbb" };
    }
    return { state: { ...state, color: body.color }, status: 200 };
}

// Sends state JSON to every client whose readyState === 1 (OPEN).
function broadcast(state, clients) {
    const msg = JSON.stringify(state);
    clients.forEach((client) => {
        if (client.readyState === 1) client.send(msg);
    });
}

module.exports = { isValidHexColor, loadStateFromJson, handlePowerRequest, handleColorRequest, broadcast };
