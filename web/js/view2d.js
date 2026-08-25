// 2D Canvas CAD Floor Plan Editor & CFD Visualization Engine

class View2D {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas.getContext('2d');
    this.mode = 'simulation'; // 'design' | 'simulation'
    this.cadTool = 'select';  // 'select' | 'wall' | 'window' | 'door' | 'human'

    this.currentSlice = null;
    this.sceneData = null;
    this.designData = null; // { domain, walls, windows, doors, occupants }

    this.colormapName = 'turbo';
    this.showVectors = true;
    this.showContours = true;
    this.showStreamlines = false;
    this.minTemp = 32.0;
    this.maxTemp = 37.0;

    this.streamlineParticles = [];
    this.initParticles();

    // CAD Selection & Dragging State
    this.selectedObject = null; // { type: 'wall'|'window'|'door'|'occupant', id: int }
    this.hoveredObject = null;
    this.draggingObject = null;
    this.drawingWall = null;    // { x1, z1, x2, z2 }
    this.gridSnap = 0.10;       // 10 cm CAD snap grid

    this.onSelectCallback = null;
    this.onCADActionCallback = null;
    this.onProbeCallback = null;

    this.setupEvents();
  }

  initParticles() {
    this.streamlineParticles = [];
    for (let i = 0; i < 180; ++i) {
      this.streamlineParticles.push({
        x: Math.random(),
        y: Math.random(),
        age: Math.floor(Math.random() * 80),
        maxAge: 60 + Math.floor(Math.random() * 60)
      });
    }
  }

  setMode(m) {
    this.mode = m;
    this.drawingWall = null;
    this.draggingObject = null;
    this.canvas.style.cursor = (m === 'design' && this.cadTool !== 'select') ? 'crosshair' : 'default';
  }

  setCADTool(tool) {
    this.cadTool = tool;
    this.drawingWall = null;
    this.canvas.style.cursor = (this.mode === 'design' && tool !== 'select') ? 'crosshair' : 'default';
  }

  setupEvents() {
    window.addEventListener('resize', () => this.resize());
    this.resize();

    this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
    this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
    window.addEventListener('mouseup', (e) => this.onMouseUp(e));

    this.canvas.addEventListener('mouseleave', () => {
      if (!this.draggingObject) {
        this.hoveredObject = null;
        if (this.mode === 'simulation') {
          const tip = document.getElementById('probe-tooltip');
          if (tip) tip.style.display = 'none';
        }
      }
    });

    window.addEventListener('keydown', (e) => {
      if (this.mode === 'design' && (e.key === 'Delete' || e.key === 'Backspace')) {
        if (this.selectedObject) {
          this.deleteSelected();
        }
      }
    });
  }

  resize() {
    const rect = this.canvas.parentElement.getBoundingClientRect();
    this.canvas.width = Math.max(100, rect.width);
    this.canvas.height = Math.max(100, rect.height);
  }

  snap(val) {
    return Math.round(val / this.gridSnap) * this.gridSnap;
  }

  updateData(sliceData) {
    this.currentSlice = sliceData;
  }

  updateScene(sceneData) {
    this.sceneData = sceneData;
    if (sceneData) {
      this.designData = {
        domain: { lx: sceneData.lx || 10.0, ly: sceneData.ly || 2.80, lz: sceneData.lz || 5.0 },
        walls: sceneData.walls || [],
        windows: sceneData.windows || [],
        doors: sceneData.doors || [],
        occupants: sceneData.occupants || []
      };
    }
  }

  // Coordinate transforms
  screenToWorld(sx, sy) {
    if (!this.renderTransform) return { x: 0, z: 0 };
    const t = this.renderTransform;
    const x = (sx - t.offsetX) / t.scale;
    const z = (sy - t.offsetY) / t.scale;
    return { x, z };
  }

  worldToScreen(wx, wz) {
    if (!this.renderTransform) return { x: 0, y: 0 };
    const t = this.renderTransform;
    return {
      x: t.offsetX + wx * t.scale,
      y: t.offsetY + wz * t.scale
    };
  }

  // --- Main Render Dispatch ---
  render() {
    const cw = this.canvas.width;
    const ch = this.canvas.height;
    if (!cw || !ch) return;

    this.ctx.fillStyle = '#05070a';
    this.ctx.fillRect(0, 0, cw, ch);

    const lx = (this.designData && this.designData.domain && Number.isFinite(this.designData.domain.lx)) ? this.designData.domain.lx : 10.0;
    const lz = (this.designData && this.designData.domain && Number.isFinite(this.designData.domain.lz)) ? this.designData.domain.lz : 5.0;

    const padding = 45;
    const availW = Math.max(50, cw - 2 * padding);
    const availH = Math.max(50, ch - 2 * padding);

    const scaleX = availW / lx;
    const scaleY = availH / lz;
    const scale = Math.min(scaleX, scaleY);

    const renderW = lx * scale;
    const renderH = lz * scale;
    const offsetX = padding + (availW - renderW) * 0.5;
    const offsetY = padding + (availH - renderH) * 0.5;

    this.renderTransform = { scale, renderW, renderH, offsetX, offsetY, lx, lz };

    if (this.mode === 'design') {
      // 1. Draw CAD Blueprint Grid & Ruler Axes
      this.renderCADGrid(offsetX, offsetY, renderW, renderH, lx, lz);
      // 2. Draw Walls, Casement Windows, Swing Doors, 6ft Humans
      this.renderCADGeometry();
      // 3. Draw Rubber-Band Wall if drawing
      if (this.drawingWall) {
        this.renderDrawingWall();
      }
    } else {
      // Simulation Mode: Draw Heatmap & Streamlines
      if (this.currentSlice && this.currentSlice.temperature) {
        this.renderHeatmap(this.currentSlice, offsetX, offsetY, renderW, renderH);
        if (this.showStreamlines) this.renderStreamlines(this.currentSlice, offsetX, offsetY, renderW, renderH);
        if (this.showVectors) this.renderVelocityVectors(this.currentSlice, offsetX, offsetY, renderW, renderH);
        if (this.showContours) this.renderContourLines(this.currentSlice, offsetX, offsetY, renderW, renderH);
      }
      // Overlay Architectural Geometry & Openings on top
      this.renderCADGeometry(true);
    }
  }

  // --- CAD Grid & Ruler Rendering ---
  renderCADGrid(ox, oy, rw, rh, lx, lz) {
    const ctx = this.ctx;

    // Floor area background
    ctx.fillStyle = '#0a0f18';
    ctx.fillRect(ox, oy, rw, rh);

    // Minor Grid (0.5m)
    ctx.strokeStyle = 'rgba(48, 54, 61, 0.4)';
    ctx.lineWidth = 1;
    for (let x = 0; x <= lx; x += 0.5) {
      const p = this.worldToScreen(x, 0);
      ctx.beginPath();
      ctx.moveTo(p.x, oy);
      ctx.lineTo(p.x, oy + rh);
      ctx.stroke();
    }
    for (let z = 0; z <= lz; z += 0.5) {
      const p = this.worldToScreen(0, z);
      ctx.beginPath();
      ctx.moveTo(ox, p.y);
      ctx.lineTo(ox + rw, p.y);
      ctx.stroke();
    }

    // Major Grid (1.0m) & Dimension Rulers
    ctx.strokeStyle = 'rgba(88, 166, 255, 0.2)';
    ctx.lineWidth = 1.5;
    ctx.fillStyle = '#8b949e';
    ctx.font = '10px monospace';
    ctx.textAlign = 'center';

    for (let x = 0; x <= lx; x += 1.0) {
      const p = this.worldToScreen(x, 0);
      ctx.beginPath();
      ctx.moveTo(p.x, oy);
      ctx.lineTo(p.x, oy + rh);
      ctx.stroke();
      ctx.fillText(`${x.toFixed(0)}m`, p.x, oy - 8);
    }

    ctx.textAlign = 'right';
    for (let z = 0; z <= lz; z += 1.0) {
      const p = this.worldToScreen(0, z);
      ctx.beginPath();
      ctx.moveTo(ox, p.y);
      ctx.lineTo(ox + rw, p.y);
      ctx.stroke();
      ctx.fillText(`${z.toFixed(0)}m`, ox - 8, p.y + 4);
    }

    // Outer boundary border
    ctx.strokeStyle = '#30363d';
    ctx.lineWidth = 2;
    ctx.strokeRect(ox, oy, rw, rh);
  }

  // --- Architectural Geometry CAD Drawing ---
  renderCADGeometry(isSimOverlay = false) {
    if (!this.designData) return;
    const ctx = this.ctx;

    // 1. Draw Walls
    const walls = this.designData.walls || [];
    for (const w of walls) {
      const isSelected = this.selectedObject && this.selectedObject.type === 'wall' && this.selectedObject.id === w.id;
      const isHovered = this.hoveredObject && this.hoveredObject.type === 'wall' && this.hoveredObject.id === w.id;

      const x1 = Number.isFinite(w.x1) ? w.x1 : 0;
      const z1 = Number.isFinite(w.z1) ? w.z1 : 0;
      const x2 = Number.isFinite(w.x2) ? w.x2 : 0;
      const z2 = Number.isFinite(w.z2) ? w.z2 : 0;

      const p1 = this.worldToScreen(x1, z1);
      const p2 = this.worldToScreen(x2, z2);
      const th = Math.max(4, (w.thickness || 0.20) * this.renderTransform.scale);

      ctx.beginPath();
      ctx.moveTo(p1.x, p1.y);
      ctx.lineTo(p2.x, p2.y);
      ctx.lineWidth = th;
      ctx.strokeStyle = isSelected ? '#58a6ff' : (isHovered ? '#79c0ff' : (isSimOverlay ? 'rgba(200,210,225,0.7)' : '#e6edf3'));
      ctx.lineCap = 'square';
      ctx.stroke();

      if (!isSimOverlay) {
        // Dimension tag
        const dx = x2 - x1, dz = z2 - z1;
        const len = Math.sqrt(dx * dx + dz * dz);
        const mx = (p1.x + p2.x) * 0.5;
        const my = (p1.y + p2.y) * 0.5;
        ctx.fillStyle = isSelected ? '#58a6ff' : '#8b949e';
        ctx.font = 'bold 9px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(`${len.toFixed(2)}m (H:2.8m)`, mx, my - 6);

        // Handles on endpoints if selected
        if (isSelected) {
          ctx.fillStyle = '#58a6ff';
          ctx.beginPath(); ctx.arc(p1.x, p1.y, 6, 0, 2 * Math.PI); ctx.fill();
          ctx.beginPath(); ctx.arc(p2.x, p2.y, 6, 0, 2 * Math.PI); ctx.fill();
        }
      }
    }

    // 2. Draw Windows (with Casement Opening Angle)
    const windows = this.designData.windows || [];
    for (const win of windows) {
      const isSelected = this.selectedObject && this.selectedObject.type === 'window' && this.selectedObject.id === win.id;
      const isHovered = this.hoveredObject && this.hoveredObject.type === 'window' && this.hoveredObject.id === win.id;

      const wx = (win.x !== undefined) ? win.x : ((win.box && win.box.length >= 2) ? (win.box[0] + win.box[1]) * 0.5 : 0);
      const wz = (win.z !== undefined) ? win.z : ((win.box && win.box.length >= 6) ? (win.box[4] + win.box[5]) * 0.5 : 0);
      if (!Number.isFinite(wx) || !Number.isFinite(wz)) continue;

      const p = this.worldToScreen(wx, wz);
      const wPx = Math.max(12, (win.width || 1.5) * this.renderTransform.scale);
      const openRatio = (win.open_ratio !== undefined) ? win.open_ratio : 1.0;
      const openAngle = openRatio * (Math.PI / 2);

      ctx.fillStyle = isSelected ? 'rgba(88, 166, 255, 0.4)' : (isHovered ? 'rgba(57, 197, 207, 0.4)' : 'rgba(57, 197, 207, 0.25)');
      ctx.strokeStyle = isSelected ? '#58a6ff' : '#39c5cf';
      ctx.lineWidth = 2;

      const isEW = (win.normal && Math.abs(win.normal[0]) > 0.5) || (wx < 0.1 || wx > this.renderTransform.lx - 0.1);

      if (isEW) {
        ctx.fillRect(p.x - 5, p.y - wPx * 0.5, 10, wPx);
        ctx.strokeRect(p.x - 5, p.y - wPx * 0.5, 10, wPx);

        const hingeY = p.y - wPx * 0.5;
        const sashLen = wPx * 0.9;
        const nx = (wx < this.renderTransform.lx * 0.5) ? -1 : 1;
        const tipX = p.x + nx * sashLen * Math.sin(openAngle);
        const tipY = hingeY + sashLen * Math.cos(openAngle);

        ctx.beginPath();
        ctx.moveTo(p.x, hingeY);
        ctx.lineTo(tipX, tipY);
        ctx.strokeStyle = '#39c5cf';
        ctx.lineWidth = 3;
        ctx.stroke();

        ctx.beginPath();
        ctx.setLineDash([2, 3]);
        ctx.arc(p.x, hingeY, sashLen, nx > 0 ? 0 : Math.PI, nx > 0 ? openAngle : Math.PI - openAngle, nx < 0);
        ctx.strokeStyle = 'rgba(57, 197, 207, 0.6)';
        ctx.lineWidth = 1;
        ctx.stroke();
        ctx.setLineDash([]);
      } else {
        ctx.fillRect(p.x - wPx * 0.5, p.y - 5, wPx, 10);
        ctx.strokeRect(p.x - wPx * 0.5, p.y - 5, wPx, 10);

        const hingeX = p.x - wPx * 0.5;
        const sashLen = wPx * 0.9;
        const nz = (wz < this.renderTransform.lz * 0.5) ? -1 : 1;
        const tipX = hingeX + sashLen * Math.cos(openAngle);
        const tipY = p.y + nz * sashLen * Math.sin(openAngle);

        ctx.beginPath();
        ctx.moveTo(hingeX, p.y);
        ctx.lineTo(tipX, tipY);
        ctx.strokeStyle = '#39c5cf';
        ctx.lineWidth = 3;
        ctx.stroke();

        ctx.beginPath();
        ctx.setLineDash([2, 3]);
        ctx.arc(hingeX, p.y, sashLen, 0, openAngle);
        ctx.strokeStyle = 'rgba(57, 197, 207, 0.6)';
        ctx.lineWidth = 1;
        ctx.stroke();
        ctx.setLineDash([]);
      }

      ctx.fillStyle = '#39c5cf';
      ctx.font = 'bold 9px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(`${win.name || 'Window'} (${Math.round(openRatio * 100)}%)`, p.x, p.y - 12);
    }

    // 3. Draw Doors (with Architectural 90° Swing Arc & Leaf Panel)
    const doors = this.designData.doors || [];
    for (const dor of doors) {
      const isSelected = this.selectedObject && this.selectedObject.type === 'door' && this.selectedObject.id === dor.id;
      const isHovered = this.hoveredObject && this.hoveredObject.type === 'door' && this.hoveredObject.id === dor.id;

      const dx = (dor.x !== undefined) ? dor.x : ((dor.box && dor.box.length >= 2) ? (dor.box[0] + dor.box[1]) * 0.5 : 0);
      const dz = (dor.z !== undefined) ? dor.z : ((dor.box && dor.box.length >= 6) ? (dor.box[4] + dor.box[5]) * 0.5 : 0);
      if (!Number.isFinite(dx) || !Number.isFinite(dz)) continue;

      const p = this.worldToScreen(dx, dz);
      const dPx = Math.max(12, (dor.width || 1.0) * this.renderTransform.scale);
      const openRatio = (dor.open_ratio !== undefined) ? dor.open_ratio : 1.0;
      const openAngle = openRatio * (Math.PI / 2);

      ctx.fillStyle = isSelected ? 'rgba(188, 140, 255, 0.4)' : (isHovered ? 'rgba(188, 140, 255, 0.3)' : 'rgba(188, 140, 255, 0.15)');
      ctx.strokeStyle = isSelected ? '#bc8cff' : '#a371f7';
      ctx.lineWidth = 2;

      const hingeX = p.x;
      const hingeY = p.y - dPx * 0.5;

      const leafX = hingeX + dPx * Math.sin(openAngle);
      const leafY = hingeY + dPx * Math.cos(openAngle);

      ctx.beginPath();
      ctx.setLineDash([3, 3]);
      ctx.arc(hingeX, hingeY, dPx, 0, Math.PI / 2);
      ctx.strokeStyle = 'rgba(188, 140, 255, 0.5)';
      ctx.lineWidth = 1;
      ctx.stroke();
      ctx.setLineDash([]);

      ctx.beginPath();
      ctx.moveTo(hingeX, hingeY);
      ctx.lineTo(leafX, leafY);
      ctx.strokeStyle = '#bc8cff';
      ctx.lineWidth = 3.5;
      ctx.stroke();

      ctx.fillStyle = '#bc8cff';
      ctx.beginPath();
      ctx.arc(hingeX, hingeY, 4, 0, 2 * Math.PI);
      ctx.fill();

      ctx.fillStyle = '#bc8cff';
      ctx.font = 'bold 9px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(`${dor.name || 'Door'} (${Math.round(openRatio * 100)}%)`, p.x, p.y - 12);
    }

    // 4. Draw 6-Foot Human Occupant Dummies @ 37°C
    const occupants = this.designData.occupants || [];
    for (const occ of occupants) {
      const isSelected = this.selectedObject && this.selectedObject.type === 'occupant' && this.selectedObject.id === occ.id;
      const isHovered = this.hoveredObject && this.hoveredObject.type === 'occupant' && this.hoveredObject.id === occ.id;

      const ox = (occ.x !== undefined) ? occ.x : ((occ.box && occ.box.length >= 2) ? (occ.box[0] + occ.box[1]) * 0.5 : 2.5);
      const oz = (occ.z !== undefined) ? occ.z : ((occ.box && occ.box.length >= 6) ? (occ.box[4] + occ.box[5]) * 0.5 : 2.5);
      if (!Number.isFinite(ox) || !Number.isFinite(oz)) continue;

      const p = this.worldToScreen(ox, oz);
      if (!Number.isFinite(p.x) || !Number.isFinite(p.y)) continue;

      // Glowing Thermal Halo (37°C Buoyancy source)
      const grad = ctx.createRadialGradient(p.x, p.y, 4, p.x, p.y, 22);
      grad.addColorStop(0, 'rgba(248, 81, 73, 0.8)');
      grad.addColorStop(0.5, 'rgba(248, 81, 73, 0.3)');
      grad.addColorStop(1, 'rgba(248, 81, 73, 0)');
      ctx.fillStyle = grad;
      ctx.beginPath();
      ctx.arc(p.x, p.y, 22, 0, 2 * Math.PI);
      ctx.fill();

      // Shoulder Torso Oval
      ctx.fillStyle = isSelected ? '#58a6ff' : (isHovered ? '#ff7b72' : '#f85149');
      ctx.beginPath();
      ctx.ellipse(p.x, p.y, 14, 9, 0, 0, 2 * Math.PI);
      ctx.fill();
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 1.5;
      ctx.stroke();

      // Head Circle
      ctx.fillStyle = '#ffeaa7';
      ctx.beginPath();
      ctx.arc(p.x, p.y, 6, 0, 2 * Math.PI);
      ctx.fill();
      ctx.stroke();

      // Label with 6ft Height & 37°C
      ctx.fillStyle = '#ff7b72';
      ctx.font = 'bold 9px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(`${occ.name || '6ft Dummy'} (${occ.temperature || 37.0}°C)`, p.x, p.y + 22);
    }
  }

  // --- Rubber-Band Wall Drawing ---
  renderDrawingWall() {
    if (!this.drawingWall) return;
    const ctx = this.ctx;
    const p1 = this.worldToScreen(this.drawingWall.x1, this.drawingWall.z1);
    const p2 = this.worldToScreen(this.drawingWall.x2, this.drawingWall.z2);

    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.lineWidth = 6;
    ctx.strokeStyle = '#238636';
    ctx.setLineDash([6, 4]);
    ctx.stroke();
    ctx.setLineDash([]);

    const dx = this.drawingWall.x2 - this.drawingWall.x1;
    const dz = this.drawingWall.z2 - this.drawingWall.z1;
    const len = Math.sqrt(dx * dx + dz * dz);
    ctx.fillStyle = '#3fb950';
    ctx.font = 'bold 10px monospace';
    ctx.textAlign = 'center';
    ctx.fillText(`${len.toFixed(2)}m`, (p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5 - 8);
  }

  // --- Heatmap & CFD Rendering ---
  renderHeatmap(data, ox, oy, rw, rh) {
    const ctx = this.ctx;
    const n1 = data.n1, n2 = data.n2;
    const cellW = rw / n1, cellH = rh / n2;

    const imgData = ctx.createImageData(n1, n2);
    const buf = imgData.data;

    for (let j = 0; j < n2; ++j) {
      for (let i = 0; i < n1; ++i) {
        const idx = j * n1 + i;
        const cellType = data.cell_type[idx];
        const pIdx = idx * 4;

        if (cellType === 1) { // Solid Wall
          buf[pIdx] = 30; buf[pIdx + 1] = 35; buf[pIdx + 2] = 42; buf[pIdx + 3] = 255;
        } else {
          const temp = data.temperature[idx];
          const rgb = Colormaps.getColor(this.colormapName, temp, this.minTemp, this.maxTemp);
          buf[pIdx] = rgb[0]; buf[pIdx + 1] = rgb[1]; buf[pIdx + 2] = rgb[2]; buf[pIdx + 3] = 230;
        }
      }
    }

    const offCanvas = document.createElement('canvas');
    offCanvas.width = n1; offCanvas.height = n2;
    offCanvas.getContext('2d').putImageData(imgData, 0, 0);

    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(offCanvas, ox, oy, rw, rh);
  }

  renderVelocityVectors(data, ox, oy, rw, rh) {
    const ctx = this.ctx;
    const n1 = data.n1, n2 = data.n2;
    const step = Math.max(2, Math.floor(n1 / 28));
    const cellW = rw / n1, cellH = rh / n2;

    ctx.lineWidth = 1.2;

    for (let j = 1; j < n2 - 1; j += step) {
      for (let i = 1; i < n1 - 1; i += step) {
        const idx = j * n1 + i;
        if (data.cell_type[idx] === 1) continue;

        const u = data.u[idx], v = data.v[idx];
        const spd = Math.sqrt(u * u + v * v);
        if (spd < 0.04) continue;

        const cx = ox + (i + 0.5) * cellW;
        const cy = oy + (j + 0.5) * cellH;
        const len = Math.min(cellW * 1.6, spd * 18);
        const nx = u / spd, ny = v / spd;

        const rgb = Colormaps.getColor(this.colormapName, data.temperature[idx], this.minTemp, this.maxTemp);
        ctx.strokeStyle = `rgba(${rgb[0]}, ${rgb[1]}, ${rgb[2]}, 0.85)`;
        ctx.fillStyle = ctx.strokeStyle;

        ctx.beginPath();
        ctx.moveTo(cx - nx * len * 0.4, cy - ny * len * 0.4);
        ctx.lineTo(cx + nx * len * 0.6, cy + ny * len * 0.6);
        ctx.stroke();

        // Arrow tip
        const head = 3.5;
        const tipX = cx + nx * len * 0.6, tipY = cy + ny * len * 0.6;
        ctx.beginPath();
        ctx.moveTo(tipX, tipY);
        ctx.lineTo(tipX - head * nx + head * 0.5 * ny, tipY - head * ny - head * 0.5 * nx);
        ctx.lineTo(tipX - head * nx - head * 0.5 * ny, tipY - head * ny + head * 0.5 * nx);
        ctx.fill();
      }
    }
  }

  renderStreamlines(data, ox, oy, rw, rh) {
    const ctx = this.ctx;
    ctx.lineWidth = 1.5;
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.7)';

    for (const p of this.streamlineParticles) {
      p.age++;
      if (p.age > p.maxAge || p.x < 0 || p.x > 1 || p.y < 0 || p.y > 1) {
        p.x = Math.random(); p.y = Math.random(); p.age = 0;
      }

      const gi = Math.min(data.n1 - 1, Math.max(0, Math.floor(p.x * data.n1)));
      const gj = Math.min(data.n2 - 1, Math.max(0, Math.floor(p.y * data.n2)));
      const idx = gj * data.n1 + gi;

      if (data.cell_type[idx] === 1) {
        p.age = p.maxAge; continue;
      }

      const u = data.u[idx], v = data.v[idx];
      const nextX = p.x + (u / data.l1) * 0.05;
      const nextY = p.y + (v / data.l2) * 0.05;

      const sx1 = ox + p.x * rw, sy1 = oy + p.y * rh;
      const sx2 = ox + nextX * rw, sy2 = oy + nextY * rh;

      ctx.beginPath();
      ctx.moveTo(sx1, sy1);
      ctx.lineTo(sx2, sy2);
      ctx.stroke();

      p.x = nextX; p.y = nextY;
    }
  }

  renderContourLines(data, ox, oy, rw, rh) {
    const ctx = this.ctx;
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.18)';
    ctx.lineWidth = 1;
    const n1 = data.n1, n2 = data.n2;
    const cellW = rw / n1, cellH = rh / n2;

    for (let t = Math.ceil(this.minTemp); t <= Math.floor(this.maxTemp); t += 1.0) {
      for (let j = 0; j < n2 - 1; ++j) {
        for (let i = 0; i < n1 - 1; ++i) {
          const t00 = data.temperature[j * n1 + i];
          const t10 = data.temperature[j * n1 + (i + 1)];
          const t01 = data.temperature[(j + 1) * n1 + i];

          if ((t00 - t) * (t10 - t) < 0) {
            const frac = (t - t00) / (t10 - t00);
            const x = ox + (i + frac) * cellW;
            const y = oy + (j + 0.5) * cellH;
            ctx.beginPath(); ctx.arc(x, y, 1.2, 0, 2 * Math.PI); ctx.stroke();
          }
        }
      }
    }
  }

  // --- Interactive Mouse Handlers ---
  onMouseMove(e) {
    const rect = this.canvas.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const w = this.screenToWorld(sx, sy);

    if (this.mode === 'design') {
      if (this.drawingWall) {
        this.drawingWall.x2 = this.snap(w.x);
        this.drawingWall.z2 = this.snap(w.z);
        return;
      }

      if (this.draggingObject) {
        const dx = w.x - this.draggingObject.startWorldX;
        const dz = w.z - this.draggingObject.startWorldZ;

        if (this.draggingObject.type === 'occupant') {
          const newX = this.snap(this.draggingObject.origX + dx);
          const newZ = this.snap(this.draggingObject.origZ + dz);
          this.draggingObject.currentX = newX;
          this.draggingObject.currentZ = newZ;
          if (this.onCADActionCallback) {
            this.onCADActionCallback({ cmd: 'move_occupant', id: this.draggingObject.id, x: newX, z: newZ });
          }
        } else if (this.draggingObject.type === 'window') {
          const newX = this.snap(this.draggingObject.origX + dx);
          const newZ = this.snap(this.draggingObject.origZ + dz);
          if (this.onCADActionCallback) {
            this.onCADActionCallback({ cmd: 'move_window', id: this.draggingObject.id, x: newX, y: this.draggingObject.origY, z: newZ });
          }
        } else if (this.draggingObject.type === 'door') {
          const newX = this.snap(this.draggingObject.origX + dx);
          const newZ = this.snap(this.draggingObject.origZ + dz);
          if (this.onCADActionCallback) {
            this.onCADActionCallback({ cmd: 'move_door', id: this.draggingObject.id, x: newX, z: newZ });
          }
        } else if (this.draggingObject.type === 'wall') {
          const newX1 = this.snap(this.draggingObject.origX + dx);
          const newZ1 = this.snap(this.draggingObject.origZ + dz);
          const newX2 = this.snap(this.draggingObject.origX2 + dx);
          const newZ2 = this.snap(this.draggingObject.origZ2 + dz);
          if (this.onCADActionCallback) {
            this.onCADActionCallback({ cmd: 'move_wall', id: this.draggingObject.id, x1: newX1, z1: newZ1, x2: newX2, z2: newZ2 });
          }
        }
        return;
      }

      // Hover Hit Testing in Design Mode
      this.hoveredObject = this.hitTest(w.x, w.z);
      this.canvas.style.cursor = this.hoveredObject ? 'grab' : (this.cadTool === 'select' ? 'default' : 'crosshair');
    } else {
      // Simulation Mode: Live Point Probe
      const tip = document.getElementById('probe-tooltip');
      if (tip && this.renderTransform) {
        if (w.x >= 0 && w.x <= this.renderTransform.lx && w.z >= 0 && w.z <= this.renderTransform.lz) {
          tip.style.display = 'block';
          tip.style.left = `${e.clientX + 14}px`;
          tip.style.top = `${e.clientY - 14}px`;
          if (this.onProbeCallback) {
            const elevY = (this.currentSlice && this.currentSlice.axis === 1) ? this.currentSlice.coord_val : 1.40;
            this.onProbeCallback(w.x, elevY, w.z);
          }
        } else {
          tip.style.display = 'none';
        }
      }
    }
  }

  onMouseDown(e) {
    const rect = this.canvas.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const w = this.screenToWorld(sx, sy);

    if (this.mode === 'design') {
      if (this.cadTool === 'wall') {
        const snapX = this.snap(w.x);
        const snapZ = this.snap(w.z);
        this.drawingWall = { x1: snapX, z1: snapZ, x2: snapX, z2: snapZ };
        return;
      }

      if (this.cadTool === 'window') {
        const snapX = this.snap(w.x);
        const snapZ = this.snap(w.z);
        if (this.onCADActionCallback) {
          this.onCADActionCallback({
            cmd: 'add_window',
            x: snapX, y: 1.50, z: snapZ,
            width: 1.50, height: 1.20,
            nx: (snapX < 0.1) ? 1 : ((snapX > this.renderTransform.lx - 0.1) ? -1 : 0),
            nz: (snapZ < 0.1) ? 1 : ((snapZ > this.renderTransform.lz - 0.1) ? -1 : 0),
            open_ratio: 1.0
          });
        }
        this.cadTool = 'select';
        return;
      }

      if (this.cadTool === 'door') {
        const snapX = this.snap(w.x);
        const snapZ = this.snap(w.z);
        if (this.onCADActionCallback) {
          this.onCADActionCallback({
            cmd: 'add_door',
            x: snapX, z: snapZ,
            width: 1.00, height: 2.10,
            open_ratio: 1.0
          });
        }
        this.cadTool = 'select';
        return;
      }

      if (this.cadTool === 'human') {
        const snapX = this.snap(w.x);
        const snapZ = this.snap(w.z);
        if (this.onCADActionCallback) {
          this.onCADActionCallback({
            cmd: 'add_occupant',
            x: snapX, z: snapZ,
            temperature: 37.0
          });
        }
        this.cadTool = 'select';
        return;
      }

      // Select / Move Tool
      const hit = this.hitTest(w.x, w.z);
      this.selectedObject = hit;
      if (this.onSelectCallback) this.onSelectCallback(hit);

      if (hit) {
        const ox = (hit.data.x !== undefined) ? hit.data.x : hit.data.x1;
        const oz = (hit.data.z !== undefined) ? hit.data.z : hit.data.z1;
        this.draggingObject = {
          type: hit.type,
          id: hit.id,
          startWorldX: w.x,
          startWorldZ: w.z,
          origX: ox,
          origZ: oz,
          origY: hit.data.y || 1.50,
          origX2: hit.data.x2,
          origZ2: hit.data.z2
        };
        this.canvas.style.cursor = 'grabbing';
      }
    }
  }

  onMouseUp(e) {
    if (this.mode === 'design') {
      if (this.drawingWall) {
        const w = this.drawingWall;
        const dx = w.x2 - w.x1, dz = w.z2 - w.z1;
        if (Math.sqrt(dx * dx + dz * dz) > 0.3) {
          if (this.onCADActionCallback) {
            this.onCADActionCallback({
              cmd: 'add_wall',
              x1: w.x1, z1: w.z1, x2: w.x2, z2: w.z2,
              thickness: 0.20
            });
          }
        }
        this.drawingWall = null;
        this.cadTool = 'select';
      }

      if (this.draggingObject) {
        this.draggingObject = null;
        this.canvas.style.cursor = 'default';
      }
    }
  }

  hitTest(wx, wz) {
    if (!this.designData) return null;

    // 1. Test Human Occupants
    for (const occ of (this.designData.occupants || [])) {
      const ox = (occ.x !== undefined) ? occ.x : ((occ.box && occ.box.length >= 2) ? (occ.box[0] + occ.box[1]) * 0.5 : 2.5);
      const oz = (occ.z !== undefined) ? occ.z : ((occ.box && occ.box.length >= 6) ? (occ.box[4] + occ.box[5]) * 0.5 : 2.5);
      const d = Math.sqrt((wx - ox) ** 2 + (wz - oz) ** 2);
      if (d < 0.45) return { type: 'occupant', id: occ.id, data: occ };
    }

    // 2. Test Windows
    for (const win of (this.designData.windows || [])) {
      const wx_c = (win.x !== undefined) ? win.x : ((win.box && win.box.length >= 2) ? (win.box[0] + win.box[1]) * 0.5 : 0);
      const wz_c = (win.z !== undefined) ? win.z : ((win.box && win.box.length >= 6) ? (win.box[4] + win.box[5]) * 0.5 : 0);
      const d = Math.sqrt((wx - wx_c) ** 2 + (wz - wz_c) ** 2);
      if (d < (win.width || 1.5) * 0.5 + 0.2) return { type: 'window', id: win.id, data: win };
    }

    // 3. Test Doors
    for (const dor of (this.designData.doors || [])) {
      const dx_c = (dor.x !== undefined) ? dor.x : ((dor.box && dor.box.length >= 2) ? (dor.box[0] + dor.box[1]) * 0.5 : 0);
      const dz_c = (dor.z !== undefined) ? dor.z : ((dor.box && dor.box.length >= 6) ? (dor.box[4] + dor.box[5]) * 0.5 : 0);
      const d = Math.sqrt((wx - dx_c) ** 2 + (wz - dz_c) ** 2);
      if (d < (dor.width || 1.0) * 0.5 + 0.2) return { type: 'door', id: dor.id, data: dor };
    }

    // 4. Test Walls
    for (const w of (this.designData.walls || [])) {
      const dx = w.x2 - w.x1, dz = w.z2 - w.z1;
      const l2 = dx * dx + dz * dz;
      if (l2 > 1e-6) {
        const t = Math.max(0, Math.min(1, ((wx - w.x1) * dx + (wz - w.z1) * dz) / l2));
        const px = w.x1 + t * dx, pz = w.z1 + t * dz;
        const dist = Math.sqrt((wx - px) ** 2 + (wz - pz) ** 2);
        if (dist <= (w.thickness || 0.20) + 0.15) {
          return { type: 'wall', id: w.id, data: w };
        }
      }
    }

    return null;
  }

  deleteSelected() {
    if (!this.selectedObject || !this.onCADActionCallback) return;
    const { type, id } = this.selectedObject;
    if (type === 'wall') this.onCADActionCallback({ cmd: 'delete_wall', id });
    else if (type === 'window') this.onCADActionCallback({ cmd: 'delete_window', id });
    else if (type === 'door') this.onCADActionCallback({ cmd: 'delete_door', id });
    else if (type === 'occupant') this.onCADActionCallback({ cmd: 'delete_occupant', id });
    this.selectedObject = null;
    if (this.onSelectCallback) this.onSelectCallback(null);
  }
}
