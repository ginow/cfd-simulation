// CFD Simulation Web Application Controller

class CFDApp {
  constructor() {
    this.viewMode = '2d'; // '2d' or '3d'
    this.sliceAxis = 1;   // 1 = Y-slice (top-down floor cut at height y)
    this.sliceIndex = 14;
    this.isPaused = false;
    this.colormap = 'turbo';
    this.sceneData = null;

    this.view2d = new View2D('canvas-2d');
    this.view3d = new View3D('viewport-3d-container');

    this.initUI();
    this.startPolling();
    this.renderLoop();
  }

  initUI() {
    // 1. View Mode Toggles
    const btn2d = document.getElementById('btn-view-2d');
    const btn3d = document.getElementById('btn-view-3d');
    const c2d = document.getElementById('canvas-2d');
    const c3d = document.getElementById('canvas-3d');
    const toolbar2d = document.getElementById('toolbar-2d-controls');

    btn2d.addEventListener('click', () => {
      this.viewMode = '2d';
      btn2d.classList.add('active');
      btn3d.classList.remove('active');
      c2d.style.display = 'block';
      c3d.style.display = 'none';
      if (toolbar2d) toolbar2d.style.display = 'flex';
      this.view2d.resize();
    });

    btn3d.addEventListener('click', () => {
      this.viewMode = '3d';
      btn3d.classList.add('active');
      btn2d.classList.remove('active');
      c2d.style.display = 'none';
      c3d.style.display = 'block';
      if (toolbar2d) toolbar2d.style.display = 'none';
      this.view3d.resize();
    });

    // Speed Multiplier
    const speedSelect = document.getElementById('select-sim-speed');
    if (speedSelect) {
      speedSelect.addEventListener('change', (e) => {
        this.postAction({ cmd: 'set_substeps', val: parseInt(e.target.value) });
      });
    }

    // 2. Play / Pause / Step / Reset
    const playBtn = document.getElementById('btn-play-pause');
    const stepBtn = document.getElementById('btn-step');
    const resetBtn = document.getElementById('btn-reset');
    const vtkBtn = document.getElementById('btn-export-vtk');

    playBtn.addEventListener('click', () => {
      this.postAction({ cmd: 'toggle_pause' });
    });

    stepBtn.addEventListener('click', () => {
      this.postAction({ cmd: 'step' });
    });

    resetBtn.addEventListener('click', () => {
      this.postAction({ cmd: 'reset' });
    });

    vtkBtn.addEventListener('click', () => {
      this.postAction({ cmd: 'export_vtk' });
      alert('VTK file exported successfully (indoor_cfd_thermal.vtk)!');
    });

    // 3. 2D Slice Controls
    const axisSelect = document.getElementById('select-slice-axis');
    const sliceSlider = document.getElementById('slider-slice-index');
    const sliceValDisplay = document.getElementById('slice-index-val');

    axisSelect.addEventListener('change', (e) => {
      this.sliceAxis = parseInt(e.target.value);
      if (this.sliceAxis === 0) { // X
        sliceSlider.max = 63;
        this.sliceIndex = 32;
      } else if (this.sliceAxis === 1) { // Y
        sliceSlider.max = 27;
        this.sliceIndex = 14;
      } else { // Z
        sliceSlider.max = 35;
        this.sliceIndex = 18;
      }
      sliceSlider.value = this.sliceIndex;
      sliceValDisplay.innerText = this.sliceIndex;
    });

    sliceSlider.addEventListener('input', (e) => {
      this.sliceIndex = parseInt(e.target.value);
      sliceValDisplay.innerText = this.sliceIndex;
    });

    // 4. Colormap & Visual Layer Toggles
    const colormapSelect = document.getElementById('select-colormap');
    colormapSelect.addEventListener('change', (e) => {
      this.colormap = e.target.value;
      this.view2d.colormapName = this.colormap;
      this.view3d.colormapName = this.colormap;
      this.updateColorbarGradient();
    });
    this.updateColorbarGradient();

    const chkHeatmap = document.getElementById('chk-show-vectors');
    chkHeatmap.addEventListener('change', (e) => {
      this.view2d.showVectors = e.target.checked;
    });

    const chkContours = document.getElementById('chk-show-contours');
    chkContours.addEventListener('change', (e) => {
      this.view2d.showContours = e.target.checked;
    });

    // 5. Environmental & Climate Controls
    this.bindSlider('slider-wind-speed', 'disp-wind-speed', ' m/s', (val) => {
      this.postAction({ cmd: 'set_wind_speed', val: parseFloat(val) });
    });

    this.bindSlider('slider-wind-angle', 'disp-wind-angle', '°', (val) => {
      this.postAction({ cmd: 'set_wind_angle', val: parseFloat(val) });
    });

    this.bindSlider('slider-outdoor-temp', 'disp-outdoor-temp', ' °C', (val) => {
      this.postAction({ cmd: 'set_outdoor_temp', val: parseFloat(val) });
    });

    // 6. Human Body Temperature Controls
    this.bindSlider('slider-human1-temp', 'disp-human1-temp', ' °C', (val) => {
      this.postAction({ cmd: 'set_human_temp', room: 0, val: parseFloat(val) });
    });

    this.bindSlider('slider-human2-temp', 'disp-human2-temp', ' °C', (val) => {
      this.postAction({ cmd: 'set_human_temp', room: 1, val: parseFloat(val) });
    });

    // 7. Window & Door Open State Sliders
    this.bindSlider('slider-win-w1a', 'disp-win-w1a', '%', (val) => {
      this.postAction({ cmd: 'set_window', idx: 0, open: parseFloat(val) / 100.0 });
    });
    this.bindSlider('slider-win-w1b', 'disp-win-w1b', '%', (val) => {
      this.postAction({ cmd: 'set_window', idx: 1, open: parseFloat(val) / 100.0 });
    });
    this.bindSlider('slider-win-w2a', 'disp-win-w2a', '%', (val) => {
      this.postAction({ cmd: 'set_window', idx: 2, open: parseFloat(val) / 100.0 });
    });
    this.bindSlider('slider-win-w2b', 'disp-win-w2b', '%', (val) => {
      this.postAction({ cmd: 'set_window', idx: 3, open: parseFloat(val) / 100.0 });
    });
    this.bindSlider('slider-door-d1', 'disp-door-d1', '%', (val) => {
      this.postAction({ cmd: 'set_door', idx: 0, open: parseFloat(val) / 100.0 });
    });
    this.bindSlider('slider-door-d2', 'disp-door-d2', '%', (val) => {
      this.postAction({ cmd: 'set_door', idx: 1, open: parseFloat(val) / 100.0 });
    });

    // 8. Window & Door Position Sliders
    this.bindSlider('slider-pos-w1a', 'disp-pos-w1a', ' m', (val) => {
      this.postAction({ cmd: 'move_window', idx: 0, x: 0.0, z: parseFloat(val) });
    });
    this.bindSlider('slider-pos-w1b', 'disp-pos-w1b', ' m', (val) => {
      this.postAction({ cmd: 'move_window', idx: 1, x: parseFloat(val), z: 0.0 });
    });
    this.bindSlider('slider-pos-d1', 'disp-pos-d1', ' m', (val) => {
      this.postAction({ cmd: 'move_door', idx: 0, x: 5.0, z: parseFloat(val) });
    });
    this.bindSlider('slider-pos-w2a', 'disp-pos-w2a', ' m', (val) => {
      this.postAction({ cmd: 'move_window', idx: 2, x: parseFloat(val), z: 5.0 });
    });
    this.bindSlider('slider-pos-w2b', 'disp-pos-w2b', ' m', (val) => {
      this.postAction({ cmd: 'move_window', idx: 3, x: 10.0, z: parseFloat(val) });
    });
    this.bindSlider('slider-pos-d2', 'disp-pos-d2', ' m', (val) => {
      this.postAction({ cmd: 'move_door', idx: 1, x: parseFloat(val), z: 0.0 });
    });
  }

  bindSlider(sliderId, dispId, unit, callback) {
    const slider = document.getElementById(sliderId);
    const disp = document.getElementById(dispId);
    if (!slider || !disp) return;

    slider.addEventListener('input', (e) => {
      disp.innerText = e.target.value + unit;
      callback(e.target.value);
    });

    slider.addEventListener('change', (e) => {
      callback(e.target.value);
    });
  }

  onOpeningMoved(type, idx, x, z) {
    if (type === 'window') {
      this.postAction({ cmd: 'move_window', idx: idx, x: x, z: z });
      if (idx === 0) this.updatePosSlider('slider-pos-w1a', 'disp-pos-w1a', z);
      else if (idx === 1) this.updatePosSlider('slider-pos-w1b', 'disp-pos-w1b', x);
      else if (idx === 2) this.updatePosSlider('slider-pos-w2a', 'disp-pos-w2a', x);
      else if (idx === 3) this.updatePosSlider('slider-pos-w2b', 'disp-pos-w2b', z);
    } else if (type === 'door') {
      this.postAction({ cmd: 'move_door', idx: idx, x: x, z: z });
      if (idx === 0) this.updatePosSlider('slider-pos-d1', 'disp-pos-d1', z);
      else if (idx === 1) this.updatePosSlider('slider-pos-d2', 'disp-pos-d2', x);
    }
  }

  updatePosSlider(sliderId, dispId, val) {
    const slider = document.getElementById(sliderId);
    const disp = document.getElementById(dispId);
    if (slider) slider.value = val.toFixed(2);
    if (disp) disp.innerText = val.toFixed(2) + ' m';
  }

  updateColorbarGradient() {
    const gradEl = document.getElementById('colorbar-gradient');
    if (!gradEl) return;

    let stops = [];
    for (let i = 0; i <= 10; ++i) {
      let t = i / 10;
      stops.push(`${Colormaps.getCSSColor(this.colormap, t)} ${t * 100}%`);
    }
    gradEl.style.background = `linear-gradient(to right, ${stops.join(', ')})`;
  }

  async postAction(data) {
    try {
      await fetch('/api/action', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
      });
    } catch (err) {
      console.error('Failed to post action:', err);
    }
  }

  async fetchStats() {
    try {
      const res = await fetch('/api/stats');
      if (res.ok) {
        const stats = await res.json();
        this.updateStatsUI(stats);
      }
    } catch (err) {}
  }

  updateStatsUI(s) {
    document.getElementById('stat-time').innerText = s.time.toFixed(2) + ' s';
    document.getElementById('stat-step').innerText = s.step;
    document.getElementById('stat-dt').innerText = (s.dt * 1000).toFixed(1) + ' ms';
    document.getElementById('stat-max-vel').innerText = s.max_velocity.toFixed(2) + ' m/s';
    document.getElementById('stat-avg-temp').innerText = s.avg_temp.toFixed(2) + ' °C';
    document.getElementById('stat-min-temp').innerText = s.min_temp.toFixed(1) + ' °C';
    document.getElementById('stat-max-temp').innerText = s.max_temp.toFixed(1) + ' °C';
    document.getElementById('stat-divergence').innerText = s.max_divergence.toExponential(2);
    document.getElementById('stat-compute-ms').innerText = s.compute_time_ms.toFixed(1) + ' ms';

    const playBtn = document.getElementById('btn-play-pause');
    if (s.is_paused) {
      playBtn.classList.add('paused');
      playBtn.innerHTML = '▶ Resume';
    } else {
      playBtn.classList.remove('paused');
      playBtn.innerHTML = '❚❚ Pause';
    }
  }

  async fetchSlice() {
    try {
      const res = await fetch(`/api/slice?axis=${this.sliceAxis}&index=${this.sliceIndex}`);
      if (res.ok) {
        const data = await res.json();
        this.view2d.updateData(data);
      }
    } catch (err) {}
  }

  async fetch3D() {
    try {
      const res = await fetch('/api/3d');
      if (res.ok) {
        const data = await res.json();
        this.sceneData = data;
        this.view3d.update3DState(data);
        this.view2d.updateScene(data);
      }
    } catch (err) {}
  }

  updateSidebarProbe(p) {
    document.getElementById('probe-coord').innerText = `(${p.x.toFixed(2)}, ${p.y.toFixed(2)}, ${p.z.toFixed(2)}) m`;
    document.getElementById('probe-temp').innerText = `${p.temperature.toFixed(2)} °C (${(p.temperature + 273.15).toFixed(2)} K)`;
    document.getElementById('probe-vel').innerText = `[${p.u.toFixed(2)}, ${p.v.toFixed(2)}, ${p.w.toFixed(2)}] m/s`;
    document.getElementById('probe-speed').innerText = `${p.speed.toFixed(2)} m/s`;
    document.getElementById('probe-pressure').innerText = `${p.pressure.toFixed(3)} Pa`;

    let region = "Indoor Fluid (Air)";
    if (p.cell_type === 1) region = "Solid Concrete Wall";
    if (p.cell_type === 2) region = "Inflow Window (Kerala Wind)";
    if (p.cell_type === 3) region = "Outflow Leeward Window";
    if (p.cell_type === 4) region = "Doorway Airflow Passage";
    if (p.cell_type === 5) region = "Human Body Thermal Source (37.0°C)";
    document.getElementById('probe-type').innerText = region;
  }

  startPolling() {
    // Initial fetch of scene layout
    this.fetch3D();

    // Poll stats and visualization data at regular intervals
    setInterval(() => {
      this.fetchStats();
      if (this.viewMode === '2d') {
        this.fetchSlice();
      } else {
        this.fetch3D();
      }
    }, 60);

    // Refresh scene layout periodically for opening moves
    setInterval(() => {
      if (this.viewMode === '2d') {
        this.fetch3D();
      }
    }, 500);
  }

  renderLoop() {
    requestAnimationFrame(() => this.renderLoop());
    if (this.viewMode === '2d') {
      this.view2d.render();
    } else {
      this.view3d.render();
    }
  }
}

window.addEventListener('DOMContentLoaded', () => {
  window.app = new CFDApp();
});
