'use strict';

const logic = require('../logic');

// ---- isValidHexColor ----

describe('isValidHexColor', () => {
    test('accepts valid lowercase hex', () => {
        expect(logic.isValidHexColor('#ff0000')).toBe(true);
    });

    test('accepts valid uppercase hex', () => {
        expect(logic.isValidHexColor('#AABBCC')).toBe(true);
    });

    test('rejects 3-digit shorthand', () => {
        expect(logic.isValidHexColor('#fff')).toBe(false);
    });

    test('rejects named colour', () => {
        expect(logic.isValidHexColor('red')).toBe(false);
    });

    test('rejects invalid hex digits', () => {
        expect(logic.isValidHexColor('#gggggg')).toBe(false);
    });

    test('rejects missing hash', () => {
        expect(logic.isValidHexColor('ff0000')).toBe(false);
    });
});

// ---- loadStateFromJson ----

describe('loadStateFromJson', () => {
    test('parses valid state', () => {
        const result = logic.loadStateFromJson('{"on":true,"color":"#ff0000"}');
        expect(result).toEqual({ on: true, color: '#ff0000' });
    });

    test('returns default on malformed JSON', () => {
        const result = logic.loadStateFromJson('not json {{');
        expect(result).toEqual({ on: false, color: '#ffffff' });
    });

    test('returns default when on is not a boolean', () => {
        const result = logic.loadStateFromJson('{"on":"yes","color":"#ff0000"}');
        expect(result).toEqual({ on: false, color: '#ffffff' });
    });

    test('returns default when color is missing', () => {
        const result = logic.loadStateFromJson('{"on":false}');
        expect(result).toEqual({ on: false, color: '#ffffff' });
    });
});

// ---- handlePowerRequest ----

describe('handlePowerRequest', () => {
    const initial = { on: false, color: '#ffffff' };

    test('turns on successfully', () => {
        const result = logic.handlePowerRequest(initial, { on: true });
        expect(result.status).toBe(200);
        expect(result.state.on).toBe(true);
        expect(result.error).toBeUndefined();
    });

    test('turns off successfully', () => {
        const result = logic.handlePowerRequest({ on: true, color: '#ff0000' }, { on: false });
        expect(result.status).toBe(200);
        expect(result.state.on).toBe(false);
    });

    test('rejects missing on field', () => {
        const result = logic.handlePowerRequest(initial, {});
        expect(result.status).toBe(400);
        expect(result.error).toBeDefined();
        expect(result.state).toEqual(initial);
    });

    test('rejects non-boolean on', () => {
        const result = logic.handlePowerRequest(initial, { on: 'yes' });
        expect(result.status).toBe(400);
        expect(result.error).toBeDefined();
    });

    test('does not mutate original state', () => {
        const state = { on: false, color: '#ffffff' };
        logic.handlePowerRequest(state, { on: true });
        expect(state.on).toBe(false);
    });
});

// ---- handleColorRequest ----

describe('handleColorRequest', () => {
    const initial = { on: true, color: '#ffffff' };

    test('sets colour successfully', () => {
        const result = logic.handleColorRequest(initial, { color: '#ff8000' });
        expect(result.status).toBe(200);
        expect(result.state.color).toBe('#ff8000');
        expect(result.error).toBeUndefined();
    });

    test('rejects 3-digit shorthand', () => {
        const result = logic.handleColorRequest(initial, { color: '#fff' });
        expect(result.status).toBe(400);
        expect(result.error).toBeDefined();
        expect(result.state).toEqual(initial);
    });

    test('rejects named colour', () => {
        const result = logic.handleColorRequest(initial, { color: 'red' });
        expect(result.status).toBe(400);
        expect(result.error).toBeDefined();
    });

    test('rejects missing color field', () => {
        const result = logic.handleColorRequest(initial, {});
        expect(result.status).toBe(400);
        expect(result.error).toBeDefined();
    });
});

// ---- broadcast ----

describe('broadcast', () => {
    function makeClient(readyState) {
        return { readyState, send: jest.fn() };
    }

    const state = { on: true, color: '#00ff00' };

    test('sends to open clients', () => {
        const client = makeClient(1);
        logic.broadcast(state, [client]);
        expect(client.send).toHaveBeenCalledWith(JSON.stringify(state));
    });

    test('skips closed clients', () => {
        const client = makeClient(3); // CLOSED
        logic.broadcast(state, [client]);
        expect(client.send).not.toHaveBeenCalled();
    });

    test('handles mixed open and closed', () => {
        const open   = makeClient(1);
        const closed = makeClient(3);
        logic.broadcast(state, [open, closed]);
        expect(open.send).toHaveBeenCalledTimes(1);
        expect(closed.send).not.toHaveBeenCalled();
    });
});
