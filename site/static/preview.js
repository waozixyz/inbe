(() => {
  const button = document.getElementById('preview-toggle');
  const label = document.getElementById('breath-label');
  const timer = document.getElementById('breath-time');
  const rings = document.getElementById('breathing-rings');
  const status = document.getElementById('preview-status');
  let elapsed = 0;
  let startedAt = 0;
  let interval = null;

  function render() {
    const seconds = Math.min(60, elapsed + (interval ? (Date.now() - startedAt) / 1000 : 0));
    const remaining = Math.ceil(60 - seconds);
    timer.textContent = '0' + Math.floor(remaining / 60) + ':' + String(remaining % 60).padStart(2, '0');
    const inhale = Math.floor(seconds / 4) % 2 === 0;
    label.textContent = inhale ? 'Breathe in' : 'Breathe out';
    rings.classList.toggle('inhale', inhale);
    if (seconds >= 60) {
      clearInterval(interval);
      interval = null;
      elapsed = 0;
      rings.classList.remove('inhale');
      label.textContent = 'A little more room. Just for you.';
      button.textContent = 'Try again';
      button.setAttribute('aria-pressed', 'false');
      status.textContent = 'Your one-minute preview is complete.';
    }
  }

  button.addEventListener('click', () => {
    if (interval) {
      elapsed += (Date.now() - startedAt) / 1000;
      clearInterval(interval);
      interval = null;
      button.textContent = 'Continue preview';
      button.setAttribute('aria-pressed', 'false');
      label.textContent = 'Take your time';
      status.textContent = 'Preview paused.';
    } else {
      startedAt = Date.now();
      interval = setInterval(render, 250);
      button.textContent = 'Pause preview';
      button.setAttribute('aria-pressed', 'true');
      status.textContent = 'Preview started. Breathe at a comfortable pace.';
      render();
    }
  });

  document.addEventListener('visibilitychange', () => {
    if (document.hidden && interval) button.click();
  });
})();
