// Integration add-on for Subgroup B's integrated page (served by hms_live only;
// the page's own files are unchanged). The page redraws only after a button
// press; this redraws it every 2 s, so ADC-3's dispenses and alarms appear on
// their own. It skips a redraw while a field has focus so typing isn't disturbed.
"use strict";

setInterval(() => {
  const active = document.activeElement;
  if (document.hidden || (active && /^(INPUT|SELECT|TEXTAREA)$/.test(active.tagName))) return;
  refresh();   // int.js
}, 2000);
