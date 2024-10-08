"use strict";

var cmd_or_ctrl = (process.platform === "darwin") ? "cmd" : "ctrl";

exports.menu = {
  "new":   { key: "n", modifiers: cmd_or_ctrl },
  "open":   { key: "o", modifiers: cmd_or_ctrl },
  "save":   { key: "s", modifiers: cmd_or_ctrl },
  "saveas": { key: "s", modifiers: cmd_or_ctrl + "+shift" },
  "print":  { key: "p", modifiers: cmd_or_ctrl + "+shift" },
  "message" : { key: "m", modifiers: cmd_or_ctrl },
  "close":  { key: "w", modifiers: cmd_or_ctrl },
  "quit":   { key: "q", modifiers: cmd_or_ctrl },

  "undo":   { key: "z", modifiers: cmd_or_ctrl },
  "redo":   { key: "z", modifiers: cmd_or_ctrl + "+shift" },
  "selectall":{ key: "a", modifiers: cmd_or_ctrl },
  "cut":    { key: "x", modifiers: cmd_or_ctrl },
  "copy":   { key: "c", modifiers: cmd_or_ctrl },
  "paste":  { key: "v", modifiers: cmd_or_ctrl },
  "paste_clipboard": { key: "v", modifiers: cmd_or_ctrl + "+alt" },
  "duplicate": { key: "d", modifiers: cmd_or_ctrl },
  "undo":   { key: "z", modifiers: cmd_or_ctrl },

  "reselect": { key: String.fromCharCode(10), modifiers: cmd_or_ctrl },
  "clear_console": { key: "l", modifiers: cmd_or_ctrl + "+shift" },
  "encapsulate": { key: "e", modifiers: cmd_or_ctrl + "+shift" },
  "tidyup": { key: "y", modifiers: cmd_or_ctrl },
  "cordinspector":   { key: "r", modifiers: cmd_or_ctrl + "+shift" },
  "find":   { key: "f", modifiers: cmd_or_ctrl },
  "findagain":{ key: "g", modifiers: cmd_or_ctrl },
  "editmode": { key: "e", modifiers: cmd_or_ctrl },
  "preferences": { key: (process.platform === "darwin") ? "," : "p",
    modifiers: cmd_or_ctrl },

  "zoomin": { key: "=", modifiers: cmd_or_ctrl },
  "zoomout": { key: "-", modifiers: cmd_or_ctrl },
  // uncomment this for AZERTY keyboards:
  //"zoomout": { key: "6", modifiers: cmd_or_ctrl },
  "zoomreset": { key: "0", modifiers: cmd_or_ctrl },
  "zoomoptimal": { key: "9", modifiers: cmd_or_ctrl },
  "zoomhoriz": { key: "9", modifiers: cmd_or_ctrl + "+alt" },
  "zoomvert": { key: "9", modifiers: cmd_or_ctrl + "+shift" },
  "fullscreen": { key: (process.platform === "darwin") ? "f" : "F11",
    modifiers: (process.platform === "darwin") ? "cmd+ctrl" : null },

  // ico@vt.edu 2022-08-11: replaced cmd with ctrl for shortcuts
  // using numbers 1-5 (also valid for 6-9) on OSX only, because
  // newer nw.js for an unknown upstream reason does not recognise
  // cmd+1 - cmd+9 keys
  "object": { key: "1", modifiers: cmd_or_ctrl },
  "msgbox": { key: "2", modifiers: cmd_or_ctrl },
  "number": { key: "3", modifiers: cmd_or_ctrl },
  "symbol": { key: "4", modifiers: cmd_or_ctrl },
  "list": {key: "5", modifiers: cmd_or_ctrl },
  "comment": { key: "6", modifiers: cmd_or_ctrl },
  "dropdown": { key: "m", modifiers: cmd_or_ctrl + "+shift" },
  "bang": { key: "b", modifiers: cmd_or_ctrl + "+shift" },
  "toggle": { key: "t", modifiers: cmd_or_ctrl + "+shift" },
  "number2": { key: "n", modifiers: cmd_or_ctrl + "+shift" },
  "vslider": { key: "v", modifiers: cmd_or_ctrl + "+shift" },
  "hslider": { key: "h", modifiers: cmd_or_ctrl + "+shift" },
  "knob": { key: "k", modifiers: cmd_or_ctrl + "+shift" },
  "vradio": { key: "d", modifiers: cmd_or_ctrl + "+shift" },
  "hradio": { key: "i", modifiers: cmd_or_ctrl + "+shift" },
  "vu":     { key: "u", modifiers: cmd_or_ctrl + "+shift" },
  "cnv": { key: "c", modifiers: cmd_or_ctrl + "+shift" },
  "image": { key: "g", modifiers: cmd_or_ctrl + "+shift" },

  "nextwin": { key: "PageDown", modifiers: cmd_or_ctrl },
  "prevwin": { key: "PageUp", modifiers: cmd_or_ctrl },
  "pdwin": { key: "r", modifiers: cmd_or_ctrl },

  "audio_on": { key: "/", modifiers: cmd_or_ctrl },
  "audio_off": { key: ".", modifiers: cmd_or_ctrl },

  "browser": { key: "b", modifiers: cmd_or_ctrl },
  "audio_off": { key: ".", modifiers: cmd_or_ctrl },
  "audio_off": { key: ".", modifiers: cmd_or_ctrl },
  "audio_off": { key: ".", modifiers: cmd_or_ctrl },
}
