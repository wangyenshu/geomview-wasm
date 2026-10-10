/*
 * geomview-env.js (--pre-js) -- what geomview.sh normally exports before
 * starting gvx.  The data directory is preloaded at /usr/share/geomview;
 * Geomview starts in its geom/ directory so the Files panel lists models.
 */
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  var data = "/usr/share/geomview";
  ENV.GEOMDATA = data;
  ENV.GEOMVIEW_DATA_DIR = data;
  ENV.GEOMVIEW_LOAD_PATH = ".:" + data + "/geom:" + data;
  ENV.GEOMVIEW_SYSTEM_INITFILE = data + "/.geomview";
  ENV.GEOMVIEW_DOC_DIR = "/usr/share/doc/geomview";
  ENV.LC_NUMERIC = "POSIX";
});
(function () {
  var prev = Module.onRuntimeInitialized;
  Module.onRuntimeInitialized = function () {
    try { FS.chdir("/usr/share/geomview/geom"); } catch (e) {}
    if (prev) prev();
  };
})();
