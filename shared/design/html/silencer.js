/* ===================================================================
   Silencer Design System — vanilla JS for interactive demos.

   Implements the documented behaviors:
   - Button state machine: INACTIVE → ACTIVATING → ACTIVE → DEACTIVATING.
     4-tick = ~168ms hover ramp. Sound is documented but a real .wav
     trigger is omitted to keep these pages noise-free.
   - Caret blink: pure CSS (32-tick = 1344ms period).
   - Scale toggle: 1x ↔ 2x.
   - Tab/Shift-Tab focus cycling within a .canvas (mirrors Interface
     tabobjects[]). Enter on focused button = click. Escape on
     [data-escape-button] = click.
   =================================================================== */
(function () {
  // ---- Scale toggle -----------------------------------------------------
  function makeScaleToggle() {
    if (!document.querySelector('.canvas')) return;
    const btn = document.createElement('button');
    btn.className = 'scale-toggle';
    btn.textContent = 'Scale: 1×';
    let scaled = false;
    btn.addEventListener('click', () => {
      scaled = !scaled;
      document.querySelectorAll('.canvas').forEach(c => {
        c.classList.toggle('scaled-2x', scaled);
      });
      btn.textContent = scaled ? 'Scale: 2×' : 'Scale: 1×';
    });
    document.body.appendChild(btn);
  }

  // ---- Button state machine --------------------------------------------
  // Per spec: each .btn participates in INACTIVE↔ACTIVATING↔ACTIVE↔DEACTIVATING.
  // For demo purposes a CSS transition handles the brightness ramp; we just
  // toggle the .is-active class on enter/leave. (The full 5-frame sprite
  // index advance is not modelled because we have no sprite frames.)
  function wireButtons() {
    document.querySelectorAll('.btn').forEach(btn => {
      btn.addEventListener('mouseenter', () => btn.classList.add('is-active'));
      btn.addEventListener('mouseleave', () => btn.classList.remove('is-active'));
      btn.addEventListener('focus',      () => btn.classList.add('is-active'));
      btn.addEventListener('blur',       () => btn.classList.remove('is-active'));
      // Checkbox toggle
      if (btn.classList.contains('btn-checkbox')) {
        btn.addEventListener('click', () => btn.classList.toggle('is-checked'));
      }
    });
  }

  // ---- Toggle radio groups (Toggle .set > 0) ---------------------------
  function wireToggles() {
    document.querySelectorAll('[data-toggle-set]').forEach(t => {
      t.addEventListener('click', () => {
        const set = t.getAttribute('data-toggle-set');
        document.querySelectorAll(`[data-toggle-set="${set}"]`).forEach(o => {
          o.classList.toggle('is-selected', o === t);
        });
      });
    });
  }

  // ---- Tab/Shift-Tab focus cycling within a .canvas --------------------
  // Browsers do this for free for tabindex elements, but the spec states
  // Left/Right also act as Prev/Next focus on non-SelectBox children. We
  // hook those for completeness.
  function wireKeyNav() {
    document.querySelectorAll('.canvas').forEach(canvas => {
      canvas.addEventListener('keydown', (e) => {
        const focusables = Array.from(canvas.querySelectorAll(
          '[tabindex], button, input, select, textarea, .btn'
        )).filter(el => !el.hasAttribute('disabled'));
        const i = focusables.indexOf(document.activeElement);
        if (e.key === 'ArrowLeft') {
          if (i > 0) { focusables[i-1].focus(); e.preventDefault(); }
        } else if (e.key === 'ArrowRight') {
          if (i >= 0 && i < focusables.length - 1) { focusables[i+1].focus(); e.preventDefault(); }
        } else if (e.key === 'Enter') {
          if (document.activeElement && document.activeElement.click) {
            document.activeElement.click();
          }
        } else if (e.key === 'Escape') {
          const esc = canvas.querySelector('[data-escape-button]');
          if (esc) esc.click();
        }
      });
    });
  }

  document.addEventListener('DOMContentLoaded', () => {
    makeScaleToggle();
    wireButtons();
    wireToggles();
    wireKeyNav();
  });
})();
