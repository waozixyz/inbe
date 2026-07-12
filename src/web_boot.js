var statusElement = document.querySelector('#status');
var progressElement = document.querySelector('#progress');
var loadingScreen = document.querySelector('#loading-screen');

function storageOriginLooksPersistent() {
  var protocol = window.location && window.location.protocol;

  return protocol === 'http:' || protocol === 'https:';
}

function reportStorageOriginProblem() {
  if (storageOriginLooksPersistent()) return;
  console.error(
    'INBE: persistent storage needs an http://localhost or https:// origin. ' +
    'Opening the web build directly as a file can prevent Firefox IndexedDB/IDBFS saves.'
  );
}

function webglAvailable() {
  var canvas;
  var gl;

  try {
    canvas = document.createElement('canvas');
    gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
  } catch (e) {
    gl = null;
  }
  return !!gl;
}

window.__inbeLoadApp = function(src) {
  var script;

  if (!webglAvailable()) {
    if (statusElement) {
      statusElement.textContent = 'WebGL is disabled. Enable WebGL in this browser to run Inner Breeze.';
    }
    if (progressElement) progressElement.hidden = true;
    console.error('INBE: WebGL is disabled or unavailable');
    return;
  }

  script = document.createElement('script');
  script.async = true;
  script.src = src;
  document.body.appendChild(script);
};

function hideLoadingScreen() {
  if (!loadingScreen) return;
  window.requestAnimationFrame(function() {
    loadingScreen.classList.add('is-hidden');
  });
}

function runStorageSync(retryDelay) {
  if (Module.__inbeStorageSyncing) return;

  Module.__inbeStorageSyncing = true;
  Module.__inbeStorageSyncPending = false;
  var shouldLog = !!Module.__inbeStorageSyncLogSuccess;
  Module.__inbeStorageSyncLogSuccess = false;

  try {
    FS.syncfs(false, function(err) {
      Module.__inbeStorageSyncing = false;
      if (err) console.error('IDBFS save failed:', err);
      else if (shouldLog) console.log('IDBFS synced');

      if (Module.__inbeStorageSyncPending) {
        if (Module.__inbeStorageSyncTimer) clearTimeout(Module.__inbeStorageSyncTimer);
        Module.__inbeStorageSyncTimer = setTimeout(function() {
          Module.__inbeStorageSyncTimer = 0;
          runStorageSync(retryDelay);
        }, retryDelay);
      }
    });
  } catch (e) {
    Module.__inbeStorageSyncing = false;
    console.error('IDBFS sync error:', e);
    if (Module.__inbeStorageSyncPending) {
      if (Module.__inbeStorageSyncTimer) clearTimeout(Module.__inbeStorageSyncTimer);
      Module.__inbeStorageSyncTimer = setTimeout(function() {
        Module.__inbeStorageSyncTimer = 0;
        runStorageSync(retryDelay);
      }, retryDelay);
    }
  }
}

function scheduleStorageSync(delay, logSuccess) {
  if (typeof FS === 'undefined' || typeof FS.syncfs !== 'function') return;
  Module.__inbeStorageSyncPending = true;
  Module.__inbeStorageSyncLogSuccess = Module.__inbeStorageSyncLogSuccess || !!logSuccess;
  if (Module.__inbeStorageSyncTimer) clearTimeout(Module.__inbeStorageSyncTimer);

  Module.__inbeStorageSyncTimer = setTimeout(function() {
    Module.__inbeStorageSyncTimer = 0;
    runStorageSync(delay);
  }, delay);
}

function flushStorageSync(logSuccess) {
  if (typeof FS === 'undefined' || typeof FS.syncfs !== 'function') return;
  Module.__inbeStorageSyncPending = true;
  Module.__inbeStorageSyncLogSuccess = Module.__inbeStorageSyncLogSuccess || !!logSuccess;
  if (Module.__inbeStorageSyncTimer) {
    clearTimeout(Module.__inbeStorageSyncTimer);
    Module.__inbeStorageSyncTimer = 0;
  }
  runStorageSync(0);
}

function flushStorageBeforePageSuspends() {
  flushStorageSync(false);
}

window.addEventListener('pagehide', flushStorageBeforePageSuspends);
window.addEventListener('beforeunload', flushStorageBeforePageSuspends);
document.addEventListener('visibilitychange', function() {
  if (document.visibilityState === 'hidden') flushStorageBeforePageSuspends();
});

var Module = {
  __inbeRuntimeReady: false,
  __inbeScheduleStorageSync: scheduleStorageSync,
  __inbeFlushStorageSync: flushStorageSync,
  preRun: [function() {
    reportStorageOriginProblem();

    if (typeof FS === 'undefined' || typeof IDBFS === 'undefined') {
      console.error('IDBFS unavailable');
      return;
    }

    try {
      FS.mkdir('/home');
    } catch (e) {}

    try {
      FS.mount(IDBFS, { root: '/' }, '/home');
    } catch (e) {
      if (e.errno !== 10 && String(e).indexOf('already mounted') === -1) {
        console.error('IDBFS mount failed:', e);
        return;
      }
    }

    addRunDependency('inbe-idbfs');
    FS.syncfs(true, function(err) {
      if (err) console.error('IDBFS init sync failed:', err);
      else console.log('IDBFS initialized');
      removeRunDependency('inbe-idbfs');
    });
  }],
  postRun: [function() {
    Module.__inbeRuntimeReady = true;
    hideLoadingScreen();
    runLaunchCommand();
  }],
  locateFile: function(path, prefix) {
    if (path === 'index.wasm' || path === 'index.data') {
      return prefix + path + '?v=WEB_CACHE_BUSTER';
    }
    return prefix + path;
  },
  print: function(text) {
    if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
    console.log(text);
  },
  printErr: function(text) {
    if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
    console.error(text);
  },
  canvas: (function() {
    var canvas = document.getElementById('canvas') || document.querySelector('canvas.emscripten');
    if (!canvas) {
      canvas = document.createElement('canvas');
      canvas.className = 'emscripten';
      canvas.id = 'canvas';
      canvas.width = 320;
      canvas.height = 560;
      canvas.tabIndex = -1;
      var frame = document.getElementById('canvas-frame') || document.body;
      frame.appendChild(canvas);
    }
    canvas.addEventListener('webglcontextlost', function(e) {
      flushStorageBeforePageSuspends();
      alert('WebGL context lost. Reload the page to continue.');
      e.preventDefault();
    }, false);
    canvas.addEventListener('contextmenu', function(e) {
      var rect = canvas.getBoundingClientRect();
      var width = canvas.width || rect.width || 1;
      var height = canvas.height || rect.height || 1;

      e.preventDefault();
      Module.__inbeContextClick = {
        x: Math.round((e.clientX - rect.left) * width / Math.max(1, rect.width)),
        y: Math.round((e.clientY - rect.top) * height / Math.max(1, rect.height)),
        time: Date.now()
      };
    }, false);
    return canvas;
  })(),
  setStatus: function(text) {
    if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
    if (text === Module.setStatus.last.text) return;

    var match = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
    var now = Date.now();
    if (match && now - Module.setStatus.last.time < 30) return;

    Module.setStatus.last.time = now;
    Module.setStatus.last.text = text;

    if (match) {
      statusElement.textContent = match[1];
      progressElement.value = parseInt(match[2], 10) * 100;
      progressElement.max = parseInt(match[4], 10) * 100;
      progressElement.hidden = false;
    } else {
      statusElement.textContent = text;
      progressElement.hidden = true;
      if (!text) hideLoadingScreen();
    }
  },
  totalDependencies: 0,
  monitorRunDependencies: function(left) {
    this.totalDependencies = Math.max(this.totalDependencies, left);
    Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies - left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
  }
};
globalThis.Module = Module;

function launchPracticeId(value) {
  switch (value) {
    case 'whm':
      return 0;
    case 'meditation':
      return 1;
    case 'sun_salutation':
      return 2;
    default:
      return -1;
  }
}

function runLaunchCommand() {
  var params = new URLSearchParams(window.location.search || '');
  var launch = params.get('inbe_launch');
  var practice = params.get('practice');
  var practiceId;

  if (launch !== 'start-practice') return;
  practiceId = launchPracticeId(practice);
  if (practiceId < 0 || typeof Module._app_web_launch_practice !== 'function') return;
  Module._app_web_launch_practice(practiceId);
}

Module.setStatus('Downloading...');
window.onerror = function() {
  Module.setStatus('Exception thrown, see JavaScript console');
  Module.setStatus = function(text) {
    if (text) Module.printErr('[post-exception status] ' + text);
  };
};

(function() {
  var backgroundTimer = 0;
  var lastBackgroundTick = 0;

  function callExport(name, args) {
    if (!Module || !Module.__inbeRuntimeReady) return 0;
    var fn = Module && Module['_' + name];
    if (typeof fn !== 'function') return 0;
    try {
      return fn.apply(null, args || []);
    } catch (e) {
      console.error('Web background callback failed:', name, e);
      return 0;
    }
  }

  function playInBackgroundEnabled() {
    return callExport('app_web_get_play_in_background') ? true : false;
  }

  function resumeWebAudio() {
    var miniaudio = window.miniaudio;
    if (!miniaudio || !miniaudio.devices) return;
    miniaudio.devices.forEach(function(device) {
      if (!device || !device.webaudio || device.state !== miniaudio.device_state.started) return;
      if (device.webaudio.state !== 'running') {
        device.webaudio.resume().catch(function(error) {
          console.error('Failed to resume web audio:', error);
        });
      }
    });
  }

  function runBackgroundTick() {
    var now = performance.now();
    var elapsed = lastBackgroundTick ? now - lastBackgroundTick : 0;
    lastBackgroundTick = now;
    resumeWebAudio();
    if (elapsed > 0) callExport('app_web_background_tick', [Math.max(1, Math.round(elapsed))]);
  }

  function stopBackgroundTimer() {
    if (backgroundTimer) {
      clearInterval(backgroundTimer);
      backgroundTimer = 0;
    }
    lastBackgroundTick = 0;
  }

  function startBackgroundTimer() {
    if (backgroundTimer || !playInBackgroundEnabled()) return;
    lastBackgroundTick = performance.now();
    runBackgroundTick();
    backgroundTimer = setInterval(runBackgroundTick, 250);
  }

  function applyVisibilityState() {
    var hidden = document.hidden;
    if (hidden) {
      callExport('app_web_set_backgrounded', [1]);
      startBackgroundTimer();
    } else {
      runBackgroundTick();
      stopBackgroundTimer();
      callExport('app_web_set_backgrounded', [0]);
      resumeWebAudio();
    }
  }

  document.addEventListener('visibilitychange', applyVisibilityState, false);
  window.addEventListener('pagehide', function() {
    callExport('app_web_set_backgrounded', [1]);
    runBackgroundTick();
  }, false);
  window.addEventListener('pageshow', applyVisibilityState, false);
  window.addEventListener('blur', function() {
    if (document.hidden) applyVisibilityState();
  }, false);
  window.addEventListener('focus', applyVisibilityState, false);
})();
