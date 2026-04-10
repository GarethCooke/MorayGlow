'use strict';

const Morray = require('../js/morray');

describe('buildWsUrl', () => {
    test('uses wss for https', () => {
        expect(Morray.buildWsUrl('https:', 'device.local')).toBe('wss://device.local/ws');
    });

    test('uses ws for http', () => {
        expect(Morray.buildWsUrl('http:', 'localhost:3000')).toBe('ws://localhost:3000/ws');
    });

    test('uses ws for any non-https protocol', () => {
        expect(Morray.buildWsUrl('ftp:', 'example.com')).toBe('ws://example.com/ws');
    });
});

describe('isValidHexColor', () => {
    test('accepts a valid lowercase hex', () => {
        expect(Morray.isValidHexColor('#ff0000')).toBe(true);
    });

    test('accepts a valid uppercase hex', () => {
        expect(Morray.isValidHexColor('#FF0000')).toBe(true);
    });

    test('accepts mixed case hex', () => {
        expect(Morray.isValidHexColor('#aAbBcC')).toBe(true);
    });

    test('rejects 3-digit shorthand', () => {
        expect(Morray.isValidHexColor('#FFF')).toBe(false);
    });

    test('rejects named colour', () => {
        expect(Morray.isValidHexColor('red')).toBe(false);
    });

    test('rejects invalid hex digits', () => {
        expect(Morray.isValidHexColor('#gggggg')).toBe(false);
    });

    test('rejects missing hash', () => {
        expect(Morray.isValidHexColor('ff0000')).toBe(false);
    });

    test('rejects empty string', () => {
        expect(Morray.isValidHexColor('')).toBe(false);
    });
});
