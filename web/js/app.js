// Master Controller: CFD Simulation & Interactive Architectural CAD Platform

class CFDApp {
  constructor() {
    this.mode = 'simulation'; // 'design' | 'simulation'
    this.viewMode = '2d';     // '2d' | '3d'
    this.isPaused = false;
    this.simSpeed = 4;

    this.sliceAxis = 1;
    this.sliceIndex = 14;
    this.colormap = 'turbo';

    this.view2d = new View2D('canvas-2d');
    this.view3d = new View3D('viewport-3d-container');

    this.stats = null;
    this.sceneData = null;
    this.selectedObject = null;

    this.init();
  }

  init() {
    this.bindEvents();
    this.setupCADCallbacks();
    this.fetchDesign();
    this.fetchScene();
    this.fetchSlice();
    this.fetchStats();
    this.updateColorbarGradient();

    // Main render and poll loops
    this.startLoops();
  }

  // --- Event Bindings ---
  bindEvents() {
    // Mode Switching
    document.getElementById('btn-mode-design').addEventListener('click', () => this.setMode('design'));
    document.getElementById('btn-mode-sim').addEventListener('click', () => this.setMode('simulation'));

    // View Switching
    document.getElementById('btn-view-2d').addEventListener('click', () => this.setViewMode('2d'));
    document.getElementById('btn-view-3d').addEventListener('click', () => this.setViewMode('3d'));

    // CAD Toolbar
    const toolBtns = ['tool-select', 'tool-wall', 'tool-window', 'tool-door', 'tool-human'];
    toolBtns.forEach(id => {
      const btn = document.getElementById(id);
      if (btn) {
        btn.addEventListener('click', () => {
          toolBtns.forEach(b => document.getElementById(b)?.classList.remove('active'));
          btn.classList.add('active');
          const tool = id.replace('tool-', '');
          this.view2d.setCADTool(tool);
        });
      }
    });

    document.getElementById('btn-cad-delete')?.addEventListener('click', () => {
      this.view2d.deleteSelected();
    });

    // Design JSON Export & Import
    document.getElementById('btn-export-design')?.addEventListener('click', () => this.exportDesignJSON());
    const fileImport = document.getElementById('file-import-design');
    document.getElementById('btn-import-design')?.addEventListener('click', () => fileImport?.click());
    fileImport?.addEventListener('change', (e) => this.importDesignJSON(e));

    // Preset Buttons
    document.getElementById('btn-preset-2room')?.addEventListener('click', () => this.loadPreset('2room'));
    document.getElementById('btn-preset-studio')?.addEventListener('click', () => this.loadPreset('studio'));
    document.getElementById('btn-preset-3room')?.addEventListener('click', () => this.loadPreset('3room'));
    document.getElementById('btn-preset-corridor')?.addEventListener('click', () => this.loadPreset('corridor'));

    // Simulation Controls
    document.getElementById('select-sim-speed')?.addEventListener('change', (e) => {
      this.simSpeed = parseInt(e.target.value);
      this.postAction({ cmd: 'set_substeps', val: this.simSpeed });
    });

    document.getElementById('btn-play-pause')?.addEventListener('click', () => {
      this.isPaused = !this.isPaused;
      this.postAction({ cmd: 'toggle_pause' });
      this.updatePlayPauseBtn();
    });

    document.getElementById('btn-step')?.addEventListener('click', () => this.postAction({ cmd: 'step' }));
    document.getElementById('btn-reset')?.addEventListener('click', () => {
      this.postAction({ cmd: 'reset' });
      this.fetchScene();
      this.fetchSlice();
    });

    document.getElementById('btn-export-vtk')?.addEventListener('click', () => {
      this.postAction({ cmd: 'export_vtk' });
      alert("VTK structured dataset exported successfully as 'indoor_cfd_thermal.vtk'!");
    });

    // 2D Viewport Options
    document.getElementById('select-slice-axis')?.addEventListener('change', (e) => {
      this.sliceAxis = parseInt(e.target.value);
      this.fetchSlice();
    });

    const sliceSlider = document.getElementById('slider-slice-index');
    sliceSlider?.addEventListener('input', (e) => {
      this.sliceIndex = parseInt(e.target.value);
      const coordVal = (this.sliceAxis === 1) ? (this.sliceIndex * 0.10).toFixed(2) + 'm' : this.sliceIndex;
      document.getElementById('slice-index-val').textContent = coordVal;
      this.fetchSlice();
    });

    document.getElementById('select-colormap')?.addEventListener('change', (e) => {
      this.colormap = e.target.value;
      this.view2d.colormapName = this.colormap;
      this.view3d.colormapName = this.colormap;
      this.updateColorbarGradient();
    });

    document.getElementById('chk-show-vectors')?.addEventListener('change', (e) => {
      this.view2d.showVectors = e.target.checked;
    });

    document.getElementById('chk-show-contours')?.addEventListener('change', (e) => {
      this.view2d.showContours = e.target.checked;
    });

    // Environmental Parameters
    this.bindRangeInput('slider-outdoor-temp', 'disp-outdoor-temp', '°C', (val) => {
      this.postAction({ cmd: 'set_outdoor_temp', val });
    });

    this.bindRangeInput('slider-wind-speed', 'disp-wind-speed', 'm/s', (val) => {
      this.postAction({ cmd: 'set_wind_speed', val });
    });

    this.bindRangeInput('slider-wind-angle', 'disp-wind-angle', '°', (val) => {
      let card = '';
      if (val >= 337.5 || val < 22.5) card = ' (N)';
      else if (val >= 22.5 && val < 67.5) card = ' (NE)';
      else if (val >= 67.5 && val < 112.5) card = ' (E)';
      else if (val >= 112.5 && val < 157.5) card = ' (SE)';
      else if (val >= 157.5 && val < 202.5) card = ' (S)';
      else if (val >= 202.5 && val < 247.5) card = ' (SW)';
      else if (val >= 247.5 && val < 292.5) card = ' (W)';
      else if (val >= 292.5 && val < 337.5) card = ' (NW)';
      document.getElementById('disp-wind-angle').textContent = `${val}°${card}`;
      this.postAction({ cmd: 'set_wind_angle', val });
    }, false);
  }

  bindRangeInput(sliderId, dispId, unit, callback, appendUnit = true) {
    const el = document.getElementById(sliderId);
    const disp = document.getElementById(dispId);
    if (!el) return;
    el.addEventListener('input', (e) => {
      const val = parseFloat(e.target.value);
      if (disp && appendUnit) disp.textContent = `${val.toFixed(1)} ${unit}`;
      if (callback) callback(val);
    });
  }

  setupCADCallbacks() {
    this.view2d.onSelectCallback = (selected) => {
      this.selectedObject = selected;
      this.renderPropertyInspector(selected);
    };

    this.view2d.onCADActionCallback = (action) => {
      this.postAction(action).then(() => {
        this.fetchScene();
        this.fetchSlice();
      });
    };

    this.view2d.onProbeCallback = (x, y, z) => {
      this.fetchProbe(x, y, z);
    };
  }

  // --- Mode State Transitions ---
  setMode(m) {
    this.mode = m;
    this.view2d.setMode(m);

    const btnDesign = document.getElementById('btn-mode-design');
    const btnSim = document.getElementById('btn-mode-sim');
    const sideDesign = document.getElementById('sidebar-design-section');
    const sideSim = document.getElementById('sidebar-sim-section');
    const cadToolbar = document.getElementById('toolbar-cad');
    const simControls = document.getElementById('toolbar-2d-controls');
    const colorbar = document.getElementById('colorbar-panel');
    const indicator = document.getElementById('badge-mode-indicator');

    if (m === 'design') {
      btnDesign.classList.add('active');
      btnSim.classList.remove('active');
      sideDesign.style.display = 'flex';
      sideSim.style.display = 'none';
      cadToolbar.style.display = 'flex';
      simControls.style.display = 'none';
      colorbar.style.display = 'none';
      indicator.textContent = 'Mode: Design CAD';
      indicator.style.color = '#58a6ff';
      indicator.style.borderColor = '#58a6ff';

      this.setViewMode('2d');
      if (!this.isPaused) {
        this.postAction({ cmd: 'toggle_pause' });
        this.isPaused = true;
        this.updatePlayPauseBtn();
      }
    } else {
      btnSim.classList.add('active');
      btnDesign.classList.remove('active');
      sideSim.style.display = 'flex';
      sideDesign.style.display = 'none';
      cadToolbar.style.display = 'none';
      simControls.style.display = 'flex';
      colorbar.style.display = 'flex';
      indicator.textContent = 'Mode: Simulation';
      indicator.style.color = '#3fb950';
      indicator.style.borderColor = '#3fb950';

      this.fetchScene();
      this.fetchSlice();
    }
  }

  setViewMode(vm) {
    this.viewMode = vm;
    const btn2d = document.getElementById('btn-view-2d');
    const btn3d = document.getElementById('btn-view-3d');
    const canvas2d = document.getElementById('canvas-2d');
    const view3dContainer = document.getElementById('viewport-3d-container');

    if (vm === '2d') {
      btn2d.classList.add('active');
      btn3d.classList.remove('active');
      canvas2d.style.display = 'block';
      view3dContainer.style.display = 'none';
    } else {
      btn3d.classList.add('active');
      btn2d.classList.remove('active');
      canvas2d.style.display = 'none';
      view3dContainer.style.display = 'block';
      this.view3d.resize();
    }
  }

  updatePlayPauseBtn() {
    const btn = document.getElementById('btn-play-pause');
    if (this.isPaused) {
      btn.textContent = '▶ Resume';
      btn.classList.add('paused');
    } else {
      btn.textContent = '❚❚ Pause';
      btn.classList.remove('paused');
    }
  }

  // --- Dynamic Property Inspector in Design Mode ---
  renderPropertyInspector(selected) {
    const container = document.getElementById('inspector-content');
    if (!container) return;

    if (!selected) {
      container.innerHTML = `
        <div style="font-size:0.8rem;color:var(--text-secondary);text-align:center;padding:12px 0;">
          Click on any wall, window, door, or 6ft dummy on the floor plan to edit its properties.
        </div>`;
      return;
    }

    const { type, id, data } = selected;

    if (type === 'wall') {
      const len = Math.sqrt((data.x2 - data.x1) ** 2 + (data.z2 - data.z1) ** 2);
      container.innerHTML = `
        <div class="prop-row"><span class="prop-label">Object:</span><b>${data.name || 'Wall ' + id}</b></div>
        <div class="prop-row"><span class="prop-label">Height:</span><b>2.80 m (Fixed 280cm)</b></div>
        <div class="prop-row"><span class="prop-label">Length:</span><b>${len.toFixed(2)} m</b></div>
        <div class="prop-row"><span class="prop-label">Thickness:</span><span>${(data.thickness || 0.20).toFixed(2)} m</span></div>
        <div style="margin-top:8px;display:flex;gap:6px;">
          <button id="btn-insp-del" class="btn-action" style="color:var(--accent-red);width:100%;justify-content:center;">Delete Wall</button>
        </div>`;
      document.getElementById('btn-insp-del')?.addEventListener('click', () => this.view2d.deleteSelected());
    }
    else if (type === 'window') {
      const openRatio = (data.open_ratio !== undefined) ? data.open_ratio : 1.0;
      const angleDeg = Math.round(openRatio * 90);
      container.innerHTML = `
        <div class="prop-row"><span class="prop-label">Object:</span><b>${data.name || 'Window ' + id}</b></div>
        <div class="control-row" style="margin-top:6px;">
          <div class="control-header"><span>Width (W):</span><span class="val-display" id="disp-insp-w">${(data.width || 1.5).toFixed(2)} m</span></div>
          <input type="range" id="slider-insp-w" min="0.4" max="3.5" step="0.05" value="${data.width || 1.5}" class="slider-input">
        </div>
        <div class="control-row" style="margin-top:6px;">
          <div class="control-header"><span>Height (H):</span><span class="val-display" id="disp-insp-h">${(data.height || 1.2).toFixed(2)} m</span></div>
          <input type="range" id="slider-insp-h" min="0.4" max="2.2" step="0.05" value="${data.height || 1.2}" class="slider-input">
        </div>
        <div class="control-row" style="margin-top:6px;">
          <div class="control-header"><span>Sill Elevation (Y):</span><span class="val-display" id="disp-insp-y">${(data.y || 1.5).toFixed(2)} m</span></div>
          <input type="range" id="slider-insp-y" min="0.3" max="2.4" step="0.05" value="${data.y || 1.5}" class="slider-input">
        </div>
        <div class="control-row" style="margin-top:6px;">
          <div class="control-header"><span>Casement Open:</span><span class="val-display" id="disp-insp-op">${Math.round(openRatio * 100)}% (${angleDeg}°)</span></div>
          <input type="range" id="slider-insp-op" min="0" max="100" value="${Math.round(openRatio * 100)}" class="slider-input">
        </div>
        <div style="margin-top:8px;">
          <button id="btn-insp-del" class="btn-action" style="color:var(--accent-red);width:100%;justify-content:center;">Delete Window</button>
        </div>`;

      document.getElementById('slider-insp-w')?.addEventListener('input', (e) => {
        const w = parseFloat(e.target.value);
        document.getElementById('disp-insp-w').textContent = `${w.toFixed(2)} m`;
        this.postAction({ cmd: 'resize_window', id, width: w, height: data.height || 1.2 }).then(() => this.fetchScene());
      });

      document.getElementById('slider-insp-h')?.addEventListener('input', (e) => {
        const h = parseFloat(e.target.value);
        document.getElementById('disp-insp-h').textContent = `${h.toFixed(2)} m`;
        this.postAction({ cmd: 'resize_window', id, width: data.width || 1.5, height: h }).then(() => this.fetchScene());
      });

      document.getElementById('slider-insp-y')?.addEventListener('input', (e) => {
        const y = parseFloat(e.target.value);
        document.getElementById('disp-insp-y').textContent = `${y.toFixed(2)} m`;
        this.postAction({ cmd: 'move_window', id, x: data.x, y: y, z: data.z }).then(() => this.fetchScene());
      });

      document.getElementById('slider-insp-op')?.addEventListener('input', (e) => {
        const op = parseInt(e.target.value) / 100.0;
        document.getElementById('disp-insp-op').textContent = `${Math.round(op * 100)}% (${Math.round(op * 90)}°)`;
        this.postAction({ cmd: 'set_window', id, open: op }).then(() => this.fetchScene());
      });

      document.getElementById('btn-insp-del')?.addEventListener('click', () => this.view2d.deleteSelected());
    }
    else if (type === 'door') {
      const openRatio = (data.open_ratio !== undefined) ? data.open_ratio : 1.0;
      const angleDeg = Math.round(openRatio * 90);
      container.innerHTML = `
        <div class="prop-row"><span class="prop-label">Object:</span><b>${data.name || 'Door ' + id}</b></div>
        <div class="control-row" style="margin-top:6px;">
          <div class="control-header"><span>Width (W):</span><span class="val-display" id="disp-insp-w">${(data.width || 1.0).toFixed(2)} m</span></div>
          <input type="range" id="slider-insp-w" min="0.6" max="2.2" step="0.05" value="${data.width || 1.0}" class="slider-input">
        </div>
        <div class="prop-row"><span class="prop-label">Height:</span><b>${(data.height || 2.10).toFixed(2)} m (Standard)</b></div>
        <div class="control-row" style="margin-top:6px;">
          <div class="control-header"><span>Door Swing Open:</span><span class="val-display" id="disp-insp-op">${Math.round(openRatio * 100)}% (${angleDeg}°)</span></div>
          <input type="range" id="slider-insp-op" min="0" max="100" value="${Math.round(openRatio * 100)}" class="slider-input">
        </div>
        <div style="margin-top:8px;">
          <button id="btn-insp-del" class="btn-action" style="color:var(--accent-red);width:100%;justify-content:center;">Delete Door</button>
        </div>`;

      document.getElementById('slider-insp-w')?.addEventListener('input', (e) => {
        const w = parseFloat(e.target.value);
        document.getElementById('disp-insp-w').textContent = `${w.toFixed(2)} m`;
        this.postAction({ cmd: 'resize_door', id, width: w, height: data.height || 2.10 }).then(() => this.fetchScene());
      });

      document.getElementById('slider-insp-op')?.addEventListener('input', (e) => {
        const op = parseInt(e.target.value) / 100.0;
        document.getElementById('disp-insp-op').textContent = `${Math.round(op * 100)}% (${Math.round(op * 90)}°)`;
        this.postAction({ cmd: 'set_door', id, open: op }).then(() => this.fetchScene());
      });

      document.getElementById('btn-insp-del')?.addEventListener('click', () => this.view2d.deleteSelected());
    }
    else if (type === 'occupant') {
      container.innerHTML = `
        <div class="prop-row"><span class="prop-label">Object:</span><b>6-Foot Human Dummy</b></div>
        <div class="prop-row"><span class="prop-label">Height:</span><b>6.0 ft (1.83 m)</b></div>
        <div class="prop-row"><span class="prop-label">Sensible Heat:</span><b>90.0 Watts</b></div>
        <div class="control-row" style="margin-top:6px;">
          <div class="control-header"><span>Core Temp:</span><span class="val-display" id="disp-insp-temp">${(data.temperature || 37.0).toFixed(1)} °C</span></div>
          <input type="range" id="slider-insp-temp" min="30.0" max="42.0" step="0.5" value="${data.temperature || 37.0}" class="slider-input">
        </div>
        <div style="margin-top:8px;">
          <button id="btn-insp-del" class="btn-action" style="color:var(--accent-red);width:100%;justify-content:center;">Delete Dummy</button>
        </div>`;

      document.getElementById('slider-insp-temp')?.addEventListener('input', (e) => {
        const t = parseFloat(e.target.value);
        document.getElementById('disp-insp-temp').textContent = `${t.toFixed(1)} °C`;
        this.postAction({ cmd: 'set_human_temp', id, val: t }).then(() => this.fetchScene());
      });

      document.getElementById('btn-insp-del')?.addEventListener('click', () => this.view2d.deleteSelected());
    }
  }

  // --- Dynamic Ventilation Openings List in Simulation Mode ---
  populateOpeningsList(scene) {
    const container = document.getElementById('sim-openings-list');
    if (!container || !scene) return;

    let html = '';
    (scene.windows || []).forEach((w) => {
      const op = Math.round((w.open_ratio !== undefined ? w.open_ratio : 1.0) * 100);
      const angle = Math.round(op * 0.9);
      html += `
        <div class="opening-item">
          <div class="opening-header">
            <span class="opening-title">${w.name || 'Window ' + w.id}</span>
            <span class="opening-tag ${w.is_inflow ? 'inflow' : 'outflow'}">${w.is_inflow ? 'Inflow' : 'Outflow'}</span>
          </div>
          <div class="control-header">
            <span>Casement Open:</span>
            <span class="val-display" id="disp-sim-win-${w.id}">${op}% (${angle}°)</span>
          </div>
          <input type="range" id="slider-sim-win-${w.id}" min="0" max="100" value="${op}" class="slider-input">
        </div>`;
    });

    (scene.doors || []).forEach((d) => {
      const op = Math.round((d.open_ratio !== undefined ? d.open_ratio : 1.0) * 100);
      const angle = Math.round(op * 0.9);
      html += `
        <div class="opening-item">
          <div class="opening-header">
            <span class="opening-title">${d.name || 'Door ' + d.id}</span>
            <span class="opening-tag door">Door</span>
          </div>
          <div class="control-header">
            <span>Swing Open:</span>
            <span class="val-display" id="disp-sim-dor-${d.id}">${op}% (${angle}°)</span>
          </div>
          <input type="range" id="slider-sim-dor-${d.id}" min="0" max="100" value="${op}" class="slider-input">
        </div>`;
    });

    container.innerHTML = html;

    // Attach listeners
    (scene.windows || []).forEach((w) => {
      document.getElementById(`slider-sim-win-${w.id}`)?.addEventListener('input', (e) => {
        const val = parseInt(e.target.value) / 100.0;
        document.getElementById(`disp-sim-win-${w.id}`).textContent = `${Math.round(val * 100)}% (${Math.round(val * 90)}°)`;
        this.postAction({ cmd: 'set_window', id: w.id, open: val }).then(() => this.fetchScene());
      });
    });

    (scene.doors || []).forEach((d) => {
      document.getElementById(`slider-sim-dor-${d.id}`)?.addEventListener('input', (e) => {
        const val = parseInt(e.target.value) / 100.0;
        document.getElementById(`disp-sim-dor-${d.id}`).textContent = `${Math.round(val * 100)}% (${Math.round(val * 90)}°)`;
        this.postAction({ cmd: 'set_door', id: d.id, open: val }).then(() => this.fetchScene());
      });
    });
  }

  // --- Export & Import Design JSON ---
  exportDesignJSON() {
    this.postAction({ cmd: 'get_design' });
    fetch('/api/design')
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then(data => {
        const jsonStr = JSON.stringify(data, null, 2);
        const blob = new Blob([jsonStr], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'building_design.json';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
      })
      .catch(err => console.error("Export design failed:", err));
  }

  importDesignJSON(event) {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      const content = e.target.result;
      fetch('/api/design', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: content
      })
        .then(res => {
          if (!res.ok) throw new Error(`HTTP ${res.status}`);
          return res.json();
        })
        .then(resp => {
          if (resp.status === 'ok') {
            this.fetchDesign();
            this.fetchScene();
            this.fetchSlice();
            alert("Building design imported and simulation grid re-rasterized successfully!");
          } else {
            alert("Error importing design: " + resp.msg);
          }
        })
        .catch(err => alert("Failed to import design: " + err));
    };
    reader.readAsText(file);
  }

  // --- Architectural Presets ---
  loadPreset(presetName) {
    let design = null;
    if (presetName === '2room') {
      design = {
        domain: { lx: 10.0, ly: 2.80, lz: 5.0 },
        walls: [
          { id: 1, x1: 0, z1: 0, x2: 0, z2: 5, thickness: 0.20 },
          { id: 2, x1: 0, z1: 5, x2: 10, z2: 5, thickness: 0.20 },
          { id: 3, x1: 10, z1: 5, x2: 10, z2: 0, thickness: 0.20 },
          { id: 4, x1: 10, z1: 0, x2: 0, z2: 0, thickness: 0.20 },
          { id: 5, x1: 5, z1: 0, x2: 5, z2: 5, thickness: 0.20 }
        ],
        windows: [
          { id: 1, name: "West Window", x: 0, y: 1.5, z: 2.5, width: 1.5, height: 1.2, open_ratio: 1.0 },
          { id: 2, name: "South Window", x: 2.5, y: 1.5, z: 0, width: 1.5, height: 1.2, open_ratio: 1.0 },
          { id: 3, name: "North Window", x: 7.5, y: 1.5, z: 5, width: 1.5, height: 1.2, open_ratio: 1.0 },
          { id: 4, name: "East Window", x: 10, y: 1.5, z: 2.5, width: 1.5, height: 1.2, open_ratio: 1.0 }
        ],
        doors: [
          { id: 1, name: "Partition Door", x: 5, y: 1.05, z: 2.5, width: 1.0, height: 2.1, open_ratio: 1.0 },
          { id: 2, name: "Exterior Door", x: 7, y: 1.05, z: 0, width: 1.0, height: 2.1, open_ratio: 0.5 }
        ],
        occupants: [
          { id: 1, name: "Occupant 1 (6ft @ 37°C)", x: 2.5, z: 2.5, temperature: 37.0 },
          { id: 2, name: "Occupant 2 (6ft @ 37°C)", x: 7.5, z: 2.5, temperature: 37.0 }
        ]
      };
    } else if (presetName === 'studio') {
      design = {
        domain: { lx: 10.0, ly: 2.80, lz: 5.0 },
        walls: [
          { id: 1, x1: 0, z1: 0, x2: 0, z2: 5, thickness: 0.20 },
          { id: 2, x1: 0, z1: 5, x2: 10, z2: 5, thickness: 0.20 },
          { id: 3, x1: 10, z1: 5, x2: 10, z2: 0, thickness: 0.20 },
          { id: 4, x1: 10, z1: 0, x2: 0, z2: 0, thickness: 0.20 }
        ],
        windows: [
          { id: 1, name: "Large West Window", x: 0, y: 1.5, z: 2.5, width: 2.4, height: 1.4, open_ratio: 1.0 },
          { id: 2, name: "East Outflow Window", x: 10, y: 1.5, z: 2.5, width: 2.4, height: 1.4, open_ratio: 1.0 }
        ],
        doors: [
          { id: 1, name: "Front Entry Door", x: 5, y: 1.05, z: 0, width: 1.0, height: 2.1, open_ratio: 0.0 }
        ],
        occupants: [
          { id: 1, name: "Occupant (6ft @ 37°C)", x: 5.0, z: 2.5, temperature: 37.0 }
        ]
      };
    } else if (presetName === '3room') {
      design = {
        domain: { lx: 10.0, ly: 2.80, lz: 5.0 },
        walls: [
          { id: 1, x1: 0, z1: 0, x2: 0, z2: 5, thickness: 0.20 },
          { id: 2, x1: 0, z1: 5, x2: 10, z2: 5, thickness: 0.20 },
          { id: 3, x1: 10, z1: 5, x2: 10, z2: 0, thickness: 0.20 },
          { id: 4, x1: 10, z1: 0, x2: 0, z2: 0, thickness: 0.20 },
          { id: 5, x1: 3.3, z1: 0, x2: 3.3, z2: 5, thickness: 0.20 },
          { id: 6, x1: 6.6, z1: 0, x2: 6.6, z2: 5, thickness: 0.20 }
        ],
        windows: [
          { id: 1, name: "Room 1 Window", x: 0, y: 1.5, z: 2.5, width: 1.5, height: 1.2, open_ratio: 1.0 },
          { id: 2, name: "Room 2 Window", x: 5, y: 1.5, z: 5, width: 1.5, height: 1.2, open_ratio: 1.0 },
          { id: 3, name: "Room 3 Window", x: 10, y: 1.5, z: 2.5, width: 1.5, height: 1.2, open_ratio: 1.0 }
        ],
        doors: [
          { id: 1, name: "Door 1-2", x: 3.3, y: 1.05, z: 2.5, width: 0.9, height: 2.1, open_ratio: 1.0 },
          { id: 2, name: "Door 2-3", x: 6.6, y: 1.05, z: 2.5, width: 0.9, height: 2.1, open_ratio: 1.0 }
        ],
        occupants: [
          { id: 1, name: "Room 1 Dummy (6ft)", x: 1.6, z: 2.5, temperature: 37.0 },
          { id: 2, name: "Room 2 Dummy (6ft)", x: 5.0, z: 2.5, temperature: 37.0 },
          { id: 3, name: "Room 3 Dummy (6ft)", x: 8.3, z: 2.5, temperature: 37.0 }
        ]
      };
    } else {
      design = {
        domain: { lx: 10.0, ly: 2.80, lz: 5.0 },
        walls: [
          { id: 1, x1: 0, z1: 0, x2: 0, z2: 5, thickness: 0.20 },
          { id: 2, x1: 0, z1: 5, x2: 10, z2: 5, thickness: 0.20 },
          { id: 3, x1: 10, z1: 5, x2: 10, z2: 0, thickness: 0.20 },
          { id: 4, x1: 10, z1: 0, x2: 0, z2: 0, thickness: 0.20 },
          { id: 5, x1: 0, z1: 2.0, x2: 7.0, z2: 2.0, thickness: 0.20 }
        ],
        windows: [
          { id: 1, name: "Balcony Inflow", x: 0, y: 1.5, z: 1.0, width: 1.8, height: 1.4, open_ratio: 1.0 },
          { id: 2, name: "Hall Window", x: 10, y: 1.5, z: 3.5, width: 1.8, height: 1.4, open_ratio: 1.0 }
        ],
        doors: [
          { id: 1, name: "Hall Entry Door", x: 4.0, y: 1.05, z: 2.0, width: 1.0, height: 2.1, open_ratio: 1.0 }
        ],
        occupants: [
          { id: 1, name: "Hall Occupant (6ft)", x: 5.0, z: 3.5, temperature: 37.0 }
        ]
      };
    }

    fetch('/api/design', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(design)
    })
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then(() => {
        this.fetchDesign();
        this.fetchScene();
        this.fetchSlice();
      })
      .catch(err => console.warn("loadPreset info:", err));
  }

  // --- REST API Fetchers ---
  fetchDesign() {
    fetch('/api/design')
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then(data => {
        this.designData = data;
        this.view2d.updateScene(data);
        this.view3d.update3DState(data);
        this.populateOpeningsList(data);
      })
      .catch(err => console.warn("fetchDesign info:", err));
  }

  fetchScene() {
    fetch('/api/3d')
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then(data => {
        this.sceneData = data;
        this.view2d.updateScene(data);
        this.view3d.update3DState(data);
        this.populateOpeningsList(data);
      })
      .catch(err => console.warn("fetchScene info:", err));
  }

  fetchSlice() {
    fetch(`/api/slice?axis=${this.sliceAxis}&index=${this.sliceIndex}`)
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then(data => {
        this.view2d.updateData(data);
      })
      .catch(err => console.warn("fetchSlice info:", err));
  }

  fetchStats() {
    fetch('/api/stats')
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then(data => {
        this.stats = data;
        this.updateStatsUI(data);
      })
      .catch(err => console.warn("fetchStats info:", err));
  }

  fetchProbe(x, y, z) {
    fetch(`/api/probe?x=${x.toFixed(2)}&y=${y.toFixed(2)}&z=${z.toFixed(2)}`)
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .then(data => {
        this.updateProbeUI(data);
      })
      .catch(err => console.warn("fetchProbe info:", err));
  }

  postAction(payload) {
    return fetch('/api/action', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    })
      .then(res => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        return res.json();
      })
      .catch(err => console.warn("postAction info:", err));
  }

  // --- UI Metric Formatters ---
  updateStatsUI(s) {
    if (!s) return;
    document.getElementById('stat-time').textContent = `${s.time.toFixed(2)} s`;
    document.getElementById('stat-step').textContent = s.step;
    document.getElementById('stat-dt').textContent = `${(s.dt * 1000).toFixed(1)} ms`;
    document.getElementById('stat-max-vel').textContent = `${s.max_velocity.toFixed(2)} m/s`;
    document.getElementById('stat-avg-temp').textContent = `${s.avg_temp.toFixed(2)} °C`;
    document.getElementById('stat-min-temp').textContent = s.min_temp.toFixed(1);
    document.getElementById('stat-max-temp').textContent = s.max_temp.toFixed(1);
    const divEl = document.getElementById('stat-divergence');
    if (divEl) divEl.textContent = s.max_divergence.toExponential(2);

    // Dynamic Colormap Temperature Bounds based on environmental conditions
    const outdoorT = (s.outdoor_temp !== undefined) ? s.outdoor_temp : 32.0;
    const minT = Math.min(outdoorT, s.min_temp);
    const maxT = Math.max(37.0, s.max_temp);
    this.view2d.minTemp = minT;
    this.view2d.maxTemp = (maxT > minT + 0.5) ? maxT : minT + 5.0;
    this.view3d.minTemp = this.view2d.minTemp;
    this.view3d.maxTemp = this.view2d.maxTemp;

    const minLbl = document.getElementById('colorbar-min-lbl');
    const midLbl = document.getElementById('colorbar-mid-lbl');
    const maxLbl = document.getElementById('colorbar-max-lbl');
    if (minLbl) minLbl.textContent = `${this.view2d.minTemp.toFixed(1)}°C`;
    if (midLbl) midLbl.textContent = `${((this.view2d.minTemp + this.view2d.maxTemp) * 0.5).toFixed(1)}°C`;
    if (maxLbl) maxLbl.textContent = `${this.view2d.maxTemp.toFixed(1)}°C`;

    this.isPaused = s.is_paused;
    this.updatePlayPauseBtn();
  }

  updateProbeUI(p) {
    if (!p) return;
    document.getElementById('probe-coord').textContent = `(${p.x.toFixed(2)}, ${p.y.toFixed(2)}, ${p.z.toFixed(2)}) m`;
    document.getElementById('probe-temp').textContent = `${p.temperature.toFixed(2)} °C`;
    document.getElementById('probe-vel').textContent = `[${p.u.toFixed(2)}, ${p.v.toFixed(2)}, ${p.w.toFixed(2)}]`;
    document.getElementById('probe-speed').textContent = `${p.speed.toFixed(2)} m/s`;
    document.getElementById('probe-pressure').textContent = `${p.pressure.toFixed(3)} Pa`;

    const typeNames = ['Air (Fluid)', 'Solid Wall', 'Inflow Window', 'Outflow Window', 'Door', '6ft Dummy (37°C)'];
    document.getElementById('probe-type').textContent = typeNames[p.cell_type] || 'Air';

    const tip = document.getElementById('probe-tooltip');
    if (tip) {
      tip.innerHTML = `
        <div style="color:var(--accent-blue);font-weight:700;">Probe (${p.x.toFixed(2)}, ${p.z.toFixed(2)})m</div>
        <div>Temp: <b style="color:#f85149;">${p.temperature.toFixed(2)}°C</b></div>
        <div>Speed: <b style="color:var(--accent-cyan);">${p.speed.toFixed(2)} m/s</b></div>
        <div>Region: <b>${typeNames[p.cell_type] || 'Air'}</b></div>`;
    }
  }

  updateColorbarGradient() {
    const bar = document.getElementById('colorbar-gradient');
    if (!bar) return;
    if (this.colormap === 'turbo') {
      bar.style.background = 'linear-gradient(to right, #30123b, #4145ab, #4675ed, #39a2fc, #1bcfd4, #24eca6, #61fc6c, #a4fc3b, #d1e834, #f3c63a, #fe9b2d, #f36315, #d93806, #b01503, #7a0402)';
    } else if (this.colormap === 'jet') {
      bar.style.background = 'linear-gradient(to right, #0000ff, #00ffff, #00ff00, #ffff00, #ff0000)';
    } else if (this.colormap === 'viridis') {
      bar.style.background = 'linear-gradient(to right, #440154, #3b528b, #21908d, #5dc863, #fde725)';
    } else if (this.colormap === 'inferno') {
      bar.style.background = 'linear-gradient(to right, #000004, #57106e, #bb3754, #f98e09, #fcffa4)';
    } else if (this.colormap === 'coolwarm') {
      bar.style.background = 'linear-gradient(to right, #3b4cc0, #8cb2e8, #dddcdc, #f49a7b, #b40426)';
    }
  }

  // --- Animation & Poll Loops ---
  startLoops() {
    const renderLoop = () => {
      if (this.viewMode === '2d') {
        this.view2d.render();
      } else {
        this.view3d.render();
      }
      requestAnimationFrame(renderLoop);
    };
    requestAnimationFrame(renderLoop);

    setInterval(() => {
      if (this.mode === 'simulation') {
        this.fetchSlice();
        this.fetchStats();
      }
    }, 120);

    setInterval(() => {
      if (this.viewMode === '3d' && this.mode === 'simulation') {
        this.fetchScene();
      }
    }, 400);
  }
}

// Initialize Application on Window Load
window.addEventListener('DOMContentLoaded', () => {
  window.app = new CFDApp();
});
