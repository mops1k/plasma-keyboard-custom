// SPDX-FileCopyrightText: 2026 plasma-keyboard-custom contributors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// Source-binding regression test, not a Qt/KWin input integration test.
import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
import vm from "node:vm";

const english = [
    ["`", "~"], ...Array.from("1234567890", (digit, i) => [digit, "!@#$%^&*()"[i]]),
    ["-", "_"], ["=", "+"], ["[", "{"], ["]", "}"], ["\\", "|"],
    [";", ":"], ["'", '"'], [",", "<"], [".", ">"], ["/", "?"],
];
const ukrainian = [
    ["`", "~"], ...Array.from("1234567890", (digit, i) => [digit, '!"№;%:?*()'[i]]),
    ["-", "_"], ["=", "+"], [".", ","],
];

let checks = 0;
for (const [locale, expected, letter] of [["fallback", english, "q"], ["uk_UA", ukrainian, "й"]]) {
    const source = readFileSync(new URL(`../src/layouts/${locale}/main.qml`, import.meta.url), "utf8");
    const shiftExpression = source.match(/readonly property bool symbolsShifted:\s*([^\n]+)/)?.[1];
    assert.ok(shiftExpression, `${locale}: missing shared symbol-level binding`);
    // Braces inside quoted punctuation strings are not QML block delimiters.
    const blocks = [...source.matchAll(/\bKey\s*\{(?:[^"{}]|"(?:\\.|[^"\\])*")*\}/g)].map(match => match[0]);
    const binding = (block, property) => block.match(new RegExp(`^\\s*${property}:\\s*([^\\n]+)`, "m"))?.[1];
    const evaluate = (expression, shift, caps) => {
        const context = vm.createContext({ InputContext: { shiftActive: shift, capsLockActive: caps } });
        context.pcLayout = { symbolsShifted: vm.runInContext(shiftExpression, context, { timeout: 100 }) };
        return vm.runInContext(expression, context, { timeout: 100 });
    };
    const keys = blocks.filter(block => binding(block, "text")?.includes("pcLayout.symbolsShifted"));
    assert.equal(keys.length, expected.length, `${locale}: all PC symbol keys must be Shift-aware`);
    for (const [base, shifted] of expected) {
        const matches = keys.filter(block => evaluate(binding(block, "text"), false, false) === base);
        assert.equal(matches.length, 1, `${locale}: unique base key ${JSON.stringify(base)}`);
        const key = matches[0];
        assert.equal(binding(key, "displayText"), undefined, "caption must follow actual input text");
        assert.equal(binding(key, "key"), undefined, "Qt Key must derive its symbol from current text");
        assert.equal(binding(key, "noModifier"), undefined, "do not disable Qt's normal modifiers");
        const alternatives = JSON.parse(binding(key, "alternativeKeys"));
        assert.ok(alternatives.includes(base) && alternatives.includes(shifted));
        for (const [shift, caps] of [[false, false], [true, false], [false, false], [false, true], [true, true]]) {
            const isShifted = shift && !caps;
            assert.equal(evaluate(binding(key, "text"), shift, caps), isShifted ? shifted : base);
            assert.equal(evaluate(binding(key, "smallText"), shift, caps), isShifted ? base : shifted);
            checks++;
        }
    }
    // Letter casing still belongs to Qt VK; do not hardcode a second case layer.
    assert.ok(blocks.some(block => binding(block, "text") === JSON.stringify(letter)));
}
for (const locale of ["en_US", "en_GB"]) {
    assert.equal(readFileSync(new URL(`../src/layouts/${locale}/main.fallback`, import.meta.url), "utf8").trim(), "");
}
// Enforce one positional grid across languages, including fixed/flexible widths.
function rows(source) {
    const result = [];
    const starts = /\bKeyboardRow\s*\{/g;
    for (let match; (match = starts.exec(source));) {
        let depth = 1, quote = null, escaped = false, end = starts.lastIndex;
        for (; end < source.length && depth; end++) {
            const ch = source[end];
            if (quote) {
                if (escaped) escaped = false;
                else if (ch === "\\") escaped = true;
                else if (ch === quote) quote = null;
            } else if (ch === '"' || ch === "'") quote = ch;
            else if (ch === "{") depth++;
            else if (ch === "}") depth--;
        }
        assert.equal(depth, 0, "balanced layout row");
        const row = source.slice(starts.lastIndex, end - 1);
        result.push([...row.matchAll(/\b((?:PlasmaKeyboard\.)?\w*Key)\s*\{(?:[^"{}]|"(?:\\.|[^"\\])*")*\}/g)]
            .map(match => ({ type: match[1], body: match[0] })));
        starts.lastIndex = end;
    }
    return result;
}
const layouts = ["fallback", "uk_UA"].map(locale => rows(readFileSync(new URL(`../src/layouts/${locale}/main.qml`, import.meta.url), "utf8")));
assert.deepEqual(layouts[0].map(row => row.length), [15, 14, 13, 14, 11]);
assert.deepEqual(layouts[1].map(row => row.length), layouts[0].map(row => row.length));
// The symbol page must stay reachable in every layout that carries a Win key.
for (const locale of readdirSync(new URL("../src/layouts/", import.meta.url))) {
    let qml;
    try { qml = readFileSync(new URL(`../src/layouts/${locale}/main.qml`, import.meta.url), "utf8"); } catch { continue; }
    if (qml.includes('displayText: "Win"')) {
        assert.ok(/\bSymbolModeKey\s*\{/.test(qml), `${locale}: Win key must not replace the only path to the symbol page`);
    }
}
const geometry = body => ["weight", "Layout.preferredWidth", "Layout.fillWidth"].map(property => {
    const name = property.replaceAll(".", "\\.");
    return body.match(new RegExp(`^\\s*${name}:\\s*([^\\n]+)`, "m"))?.[1].trim() ?? "inherited";
});
for (let row = 0; row < layouts[0].length; row++) {
    for (let slot = 0; slot < layouts[0][row].length; slot++) {
        const en = layouts[0][row][slot], uk = layouts[1][row][slot];
        assert.equal(uk.type, en.type, `row ${row} slot ${slot}: same key type`);
        assert.deepEqual(geometry(uk.body), geometry(en.body), `row ${row} slot ${slot}: same sizing rules`);
    }
}
const baseText = key => {
    const expression = key.body.match(/^\s*text:\s*([^\n]+)/m)?.[1];
    return expression ? vm.runInNewContext(expression, { pcLayout: { symbolsShifted: false } }) : "";
};
assert.equal(baseText(layouts[1][1].at(-1)), "ґ", "ghe belongs in the backslash slot");
assert.equal(layouts[1][3].slice(1, 11).map(baseText).join(""), "ячсмитьбю.", "no extra key after Shift or extra comma after ю");
for (const layout of layouts) assert.match(layout[1][1].body, /id:\s*normalKey\b/, "same reference-width slot");
console.log(`PASS: ${checks} synthetic Shift/Caps/release cases and EN/UK positional geometry parity`);
