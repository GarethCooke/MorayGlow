'use strict';

const Moray = require('../../MorayServer/data/js/moray');

describe('buildWsUrl', () => {
    test('uses wss for https', () => {
        expect(Moray.buildWsUrl('https:', 'device.local')).toBe('wss://device.local/ws');
    });

    test('uses ws for http', () => {
        expect(Moray.buildWsUrl('http:', 'localhost:3000')).toBe('ws://localhost:3000/ws');
    });

    test('uses ws for any non-https protocol', () => {
        expect(Moray.buildWsUrl('ftp:', 'example.com')).toBe('ws://example.com/ws');
    });
});

describe('isValidHexColor', () => {
    test('accepts a valid lowercase hex', () => {
        expect(Moray.isValidHexColor('#ff0000')).toBe(true);
    });

    test('accepts a valid uppercase hex', () => {
        expect(Moray.isValidHexColor('#FF0000')).toBe(true);
    });

    test('accepts mixed case hex', () => {
        expect(Moray.isValidHexColor('#aAbBcC')).toBe(true);
    });

    test('rejects 3-digit shorthand', () => {
        expect(Moray.isValidHexColor('#FFF')).toBe(false);
    });

    test('rejects named colour', () => {
        expect(Moray.isValidHexColor('red')).toBe(false);
    });

    test('rejects invalid hex digits', () => {
        expect(Moray.isValidHexColor('#gggggg')).toBe(false);
    });

    test('rejects missing hash', () => {
        expect(Moray.isValidHexColor('ff0000')).toBe(false);
    });

    test('rejects empty string', () => {
        expect(Moray.isValidHexColor('')).toBe(false);
    });
});
