/*
 * textshift.js (--pre-js) -- x11.wasm delivers printable keys as text
 * events with modifier state 0, so a client that maps keycode+state to a
 * keysym (Xt/Motif, XLookupString) sees "A" as "a" and "!" as "1".  Add
 * ShiftMask for characters that are shifted on the US layout used by the
 * keymap in keymap.c.
 */
(function () {
  "use strict";
  var SHIFTED = '~!@#$%^&*()_+{}|:"<>?';
  function shiftFor(cp) {
    return (cp >= 65 && cp <= 90) || SHIFTED.indexOf(String.fromCharCode(cp)) >= 0 ? 1 : 0;
  }
  function wrap(name, cpIndex, stateIndex) {
    var f = Module[name];
    if (typeof f !== "function") return;
    Module[name] = function () {
      var a = Array.prototype.slice.call(arguments);
      a[stateIndex] = (a[stateIndex] | 0) | shiftFor(a[cpIndex]);
      return f.apply(null, a);
    };
  }
  var prev = Module.onRuntimeInitialized;
  Module.onRuntimeInitialized = function () {
    wrap("_X11WasmQueueTextEventFields", 2, 3);
    wrap("_X11WasmQueueTextEventFieldsAtWindow", 2, 3);
    wrap("_X11WasmQueueTextEvent", 1, 2);
    if (prev) prev();
  };
})();
