(function (root, factory) {
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = factory();
    } else {
        root.Morray = factory();
    }
}(typeof self !== 'undefined' ? self : this, function () {
    'use strict';

    function buildWsUrl(protocol, host) {
        return (protocol === 'https:' ? 'wss' : 'ws') + '://' + host + '/ws';
    }

    function isValidHexColor(color) {
        return typeof color === 'string' && /^#[0-9a-fA-F]{6}$/.test(color);
    }

    return { buildWsUrl: buildWsUrl, isValidHexColor: isValidHexColor };
}));
