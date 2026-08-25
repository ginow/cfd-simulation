// 2D Planar CFD Slice Visualizer (Heatmaps, Vectors, Streamlines, Contours, and Interactive Drag & Drop)

class View2D {
  constructor(canvasId) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas.getContext('2d');
    this.currentSlice = null;
    this.sceneData = null;
    this.colormapName = 'turbo';
    this.showVectors = true;
    this.showContours = true;
    this.showStreamlines = false;
    this.minTemp = 32.0;
    this.maxTemp = 37.0;

    this.streamlineParticles = [];
    this.initParticles();

    // Drag and Drop state for Windows and Doors
    this.hoveredItem = null; // { type: 'window'|'door', index: int, label: str }
    this.draggingItem = null; // { type: 'window'|'door', index: int, startX, startZ, origBox, currentX, currentZ }
    this.onMoveCallback = null;

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

  setupEvents() {
    window.addEventListener('resize', () => this.resize());
    this.resize();

    this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
    this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
    window.addEventListener('mouseup', (e) => this.onMouseUp(e));

    this.canvas.addEventListener('mouseleave', () => {
      if (!this.draggingItem) {
        this.hoveredItem = null;
        this.canvas.style.cursor = 'default';
        const tip = document.getElementById('probe-tooltip');
        if (tip) tip.style.display = 'none';
      }
    });
  }

  resize() {
    const rect = this.canvas.parentElement.getBoundingClientRect();
    this.canvas.width = rect.width;
    this.canvas.height = rect.height;
  }

  updateData(sliceData) {
    this.currentSlice = sliceData;
  }

  updateScene(sceneData) {
    this.sceneData = sceneData;
  }

  render() {
    if (!this.currentSlice) return;

    const data = this.currentSlice;
    const cw = this.canvas.width;
    const ch = this.canvas.height;

    this.ctx.fillStyle = '#05070a';
    this.ctx.fillRect(0, 0, cw, ch);

    // Maintain aspect ratio with padding
    const padding = 45;
    const availW = cw - 2 * padding;
    const availH = ch - 2 * padding;

    const scaleX = availW / data.l1;
    const scaleY = availH / data.l2;
    const scale = Math.min(scaleX, scaleY);

    const renderW = data.l1 * scale;
    const renderH = data.l2 * scale;
    const offsetX = padding + (availW - renderW) * 0.5;
    const offsetY = padding + (availH - renderH) * 0.5;

    this.renderTransform = { scale, renderW, renderH, offsetX, offsetY, l1: data.l1, l2: data.l2, n1: data.n1, n2: data.n2 };

    // 1. Draw Temperature Heatmap
    this.renderHeatmap(data, offsetX, offsetY, renderW, renderH);

    // 2. Draw Streamlines (if enabled)
    if (this.showStreamlines) {
      this.renderStreamlines(data, offsetX, offsetY, scale);
    }

    // 3. Draw Velocity Vectors
    if (this.showVectors) {
      this.renderVectors(data, offsetX, offsetY, scale);
    }

    // 4. Draw Temperature Contours
    if (this.showContours) {
      this.renderContours(data, offsetX, offsetY, scale);
    }

    // 5. Draw Obstacle Geometry Overlay
    this.renderGeometryOverlay(data, offsetX, offsetY, scale);

    // 6. Draw Interactive Draggable Windows & Doors Handles
    if (data.axis === 1 && this.sceneData) { // Top-down Floor Plan
      this.renderDraggableHandles(offsetX, offsetY, scale, data.l1, data.l2);
    }

    // 7. Draw Domain Bounding Box, Rooms, and Coordinates
    this.renderAxes(data, offsetX, offsetY, renderW, renderH);
  }

  renderHeatmap(data, offsetX, offsetY, renderW, renderH) {
    if (!this.offscreenCanvas) {
      this.offscreenCanvas = document.createElement('canvas');
    }
    if (this.offscreenCanvas.width !== data.n1 || this.offscreenCanvas.height !== data.n2) {
      this.offscreenCanvas.width = data.n1;
      this.offscreenCanvas.height = data.n2;
      this.offscreenCtx = this.offscreenCanvas.getContext('2d');
      this.imgData = this.offscreenCtx.createImageData(data.n1, data.n2);
    }

    const buf = this.imgData.data;
    const n1 = data.n1;
    const n2 = data.n2;
    const tMin = this.minTemp;
    const tMax = this.maxTemp;
    const tRange = Math.max(tMax - tMin, 0.1);

    for (let j = 0; j < n2; ++j) {
      const rowIdx = (n2 - 1 - j) * n1; // Flip Y for Cartesian display
      for (let i = 0; i < n1; ++i) {
        const srcIdx = j * n1 + i;
        const cellType = data.cell_type[srcIdx];
        const pixIdx = (rowIdx + i) * 4;

        if (cellType === 1) { // Solid Wall
          buf[pixIdx] = 22;
          buf[pixIdx + 1] = 27;
          buf[pixIdx + 2] = 34;
          buf[pixIdx + 3] = 255;
        } else {
          const temp = data.temperature[srcIdx];
          const norm = Math.max(0.0, Math.min(1.0, (temp - tMin) / tRange));
          const rgb = Colormaps.getColor(this.colormapName, norm);
          buf[pixIdx] = rgb[0];
          buf[pixIdx + 1] = rgb[1];
          buf[pixIdx + 2] = rgb[2];
          buf[pixIdx + 3] = 255;
        }
      }
    }

    this.offscreenCtx.putImageData(this.imgData, 0, 0);

    this.ctx.save();
    this.ctx.imageSmoothingEnabled = true;
    this.ctx.imageSmoothingQuality = 'high';
    this.ctx.drawImage(this.offscreenCanvas, offsetX, offsetY, renderW, renderH);
    this.ctx.restore();
  }

  renderVectors(data, offsetX, offsetY, scale) {
    const step = 2; // Downsample for legible vector arrows
    const dx = data.l1 / data.n1;
    const dy = data.l2 / data.n2;
    const arrowLen = 14.0;

    this.ctx.save();
    this.ctx.lineWidth = 1.2;

    for (let j = 1; j < data.n2 - 1; j += step) {
      for (let i = 1; i < data.n1 - 1; i += step) {
        const idx = j * data.n1 + i;
        if (data.cell_type[idx] === 1) continue;

        const u = data.u[idx];
        const v = data.v[idx];
        const speed = data.speed[idx];
        if (speed < 0.04) continue;

        const cx = offsetX + (i + 0.5) * dx * scale;
        const cy = offsetY + (data.l2 - (j + 0.5) * dy) * scale; // Inverted Y

        const normU = u / speed;
        const normV = -v / speed; // screen Y inverted
        const len = Math.min(speed * arrowLen, 22.0);

        const ex = cx + normU * len;
        const ey = cy + normV * len;

        // Color arrow by speed (cyan to yellow)
        const sNorm = Math.min(speed / 3.0, 1.0);
        this.ctx.strokeStyle = `rgba(${Math.floor(60 + 195 * sNorm)}, ${Math.floor(220 + 35 * sNorm)}, ${Math.floor(240 - 200 * sNorm)}, 0.75)`;
        this.ctx.fillStyle = this.ctx.strokeStyle;

        this.ctx.beginPath();
        this.ctx.moveTo(cx, cy);
        this.ctx.lineTo(ex, ey);
        this.ctx.stroke();

        // Arrowhead
        const angle = Math.atan2(normV, normU);
        const headLen = 4.0;
        this.ctx.beginPath();
        this.ctx.moveTo(ex, ey);
        this.ctx.lineTo(ex - headLen * Math.cos(angle - Math.PI / 6), ey - headLen * Math.sin(angle - Math.PI / 6));
        this.ctx.lineTo(ex - headLen * Math.cos(angle + Math.PI / 6), ey - headLen * Math.sin(angle + Math.PI / 6));
        this.ctx.closePath();
        this.ctx.fill();
      }
    }
    this.ctx.restore();
  }

  renderStreamlines(data, offsetX, offsetY, scale) {
    const dx = data.l1 / data.n1;
    const dy = data.l2 / data.n2;

    this.ctx.save();
    this.ctx.lineWidth = 1.5;
    this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.6)';

    for (let p of this.streamlineParticles) {
      const gridX = p.x * (data.n1 - 1);
      const gridY = p.y * (data.n2 - 1);
      const i0 = Math.floor(gridX);
      const j0 = Math.floor(gridY);

      if (i0 >= 0 && i0 < data.n1 - 1 && j0 >= 0 && j0 < data.n2 - 1) {
        const idx = j0 * data.n1 + i0;
        const u = data.u[idx];
        const v = data.v[idx];

        const cx = offsetX + (p.x * data.l1) * scale;
        const cy = offsetY + ((1.0 - p.y) * data.l2) * scale;

        this.ctx.beginPath();
        this.ctx.arc(cx, cy, 1.5, 0, Math.PI * 2);
        this.ctx.fillStyle = 'rgba(255, 255, 255, 0.85)';
        this.ctx.fill();

        // Advect particle
        p.x += (u * 0.035) / data.l1;
        p.y += (v * 0.035) / data.l2;
        p.age++;

        if (p.x < 0 || p.x > 1 || p.y < 0 || p.y > 1 || p.age > p.maxAge) {
          p.x = Math.random();
          p.y = Math.random();
          p.age = 0;
        }
      } else {
        p.x = Math.random();
        p.y = Math.random();
        p.age = 0;
      }
    }
    this.ctx.restore();
  }

  renderContours(data, offsetX, offsetY, scale) {
    const dx = data.l1 / data.n1;
    const dy = data.l2 / data.n2;
    const isoTemps = [32.5, 33.0, 33.5, 34.0, 34.5, 35.0, 35.5, 36.0, 36.5];

    this.ctx.save();
    this.ctx.lineWidth = 1.0;

    for (let targetT of isoTemps) {
      this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
      this.ctx.beginPath();

      for (let j = 0; j < data.n2 - 1; ++j) {
        for (let i = 0; i < data.n1 - 1; ++i) {
          const t00 = data.temperature[j * data.n1 + i];
          const t10 = data.temperature[j * data.n1 + (i + 1)];

          if ((t00 - targetT) * (t10 - targetT) < 0) {
            const frac = (targetT - t00) / (t10 - t00);
            const px = offsetX + (i + frac + 0.5) * dx * scale;
            const py = offsetY + (data.l2 - (j + 0.5) * dy) * scale;
            this.ctx.rect(px, py, 1, 1);
          }
        }
      }
      this.ctx.stroke();
    }
    this.ctx.restore();
  }

  renderGeometryOverlay(data, offsetX, offsetY, scale) {
    const dx = data.l1 / data.n1;
    const dy = data.l2 / data.n2;

    this.ctx.save();
    for (let j = 0; j < data.n2; ++j) {
      for (let i = 0; i < data.n1; ++i) {
        const idx = j * data.n1 + i;
        const type = data.cell_type[idx];
        const px = offsetX + i * dx * scale;
        const py = offsetY + (data.l2 - (j + 1) * dy) * scale;
        const pw = dx * scale;
        const ph = dy * scale;

        if (type === 5) { // Hot Body (Human Occupant @ 37°C)
          this.ctx.fillStyle = 'rgba(248, 81, 73, 0.7)';
          this.ctx.strokeStyle = '#f85149';
          this.ctx.lineWidth = 1.5;
          this.ctx.fillRect(px, py, pw, ph);
          this.ctx.strokeRect(px, py, pw, ph);

          this.ctx.fillStyle = '#ffffff';
          this.ctx.font = 'bold 9px sans-serif';
          this.ctx.fillText('37°C', px - 2, py - 4);
        }
      }
    }
    this.ctx.restore();
  }

  // Render Interactive Draggable Handles for Windows and Doors
  renderDraggableHandles(offsetX, offsetY, scale, l1, l2) {
    if (!this.sceneData) return;

    this.ctx.save();

    // 1. Draw Windows
    if (this.sceneData.windows) {
      this.sceneData.windows.forEach((win, idx) => {
        const box = win.box; // [x_min, x_max, y_min, y_max, z_min, z_max]
        const xMin = box[0];
        const xMax = box[1];
        const zMin = box[4];
        const zMax = box[5];

        const px = offsetX + xMin * scale;
        const py = offsetY + (l2 - zMax) * scale;
        const pw = Math.max((xMax - xMin) * scale, 12);
        const ph = Math.max((zMax - zMin) * scale, 12);

        const isHovered = this.hoveredItem && this.hoveredItem.type === 'window' && this.hoveredItem.index === idx;
        const isDragging = this.draggingItem && this.draggingItem.type === 'window' && this.draggingItem.index === idx;

        // Window fill and border
        const baseColor = win.is_inflow ? '#39c5cf' : '#d29922';
        this.ctx.fillStyle = isDragging ? 'rgba(88, 166, 255, 0.85)' : (isHovered ? 'rgba(57, 197, 207, 0.75)' : (win.is_inflow ? 'rgba(57, 197, 207, 0.5)' : 'rgba(210, 153, 34, 0.5)'));
        this.ctx.strokeStyle = isDragging ? '#58a6ff' : (isHovered ? '#ffffff' : baseColor);
        this.ctx.lineWidth = isHovered || isDragging ? 3.0 : 1.5;

        this.ctx.fillRect(px, py, pw, ph);
        this.ctx.strokeRect(px, py, pw, ph);

        // Drag handle badge
        const badgeText = `☰ ${win.name.split(':')[1] || win.name}`;
        this.ctx.font = 'bold 10px sans-serif';
        const textW = this.ctx.measureText(badgeText).width;

        const badgeX = px + pw * 0.5 - textW * 0.5 - 4;
        const badgeY = py - 6;

        this.ctx.fillStyle = isDragging ? '#1f6feb' : (isHovered ? '#238636' : '#21262d');
        this.ctx.strokeStyle = '#30363d';
        this.ctx.lineWidth = 1;
        this.ctx.fillRect(badgeX, badgeY - 11, textW + 8, 14);
        this.ctx.strokeRect(badgeX, badgeY - 11, textW + 8, 14);

        this.ctx.fillStyle = '#ffffff';
        this.ctx.fillText(badgeText, badgeX + 4, badgeY);

        if (isHovered || isDragging) {
          this.ctx.fillStyle = '#e6edf3';
          this.ctx.font = '9px monospace';
          const posStr = (xMin > 0.1 && xMax < l1 - 0.1) ? `X: ${(0.5 * (xMin + xMax)).toFixed(2)}m` : `Z: ${(0.5 * (zMin + zMax)).toFixed(2)}m`;
          this.ctx.fillText(`↔ Drag (${posStr})`, badgeX + 4, badgeY + 13);
        }
      });
    }

    // 2. Draw Doors
    if (this.sceneData.doors) {
      this.sceneData.doors.forEach((dor, idx) => {
        const box = dor.box;
        const xMin = box[0];
        const xMax = box[1];
        const zMin = box[4];
        const zMax = box[5];

        const px = offsetX + xMin * scale;
        const py = offsetY + (l2 - zMax) * scale;
        const pw = Math.max((xMax - xMin) * scale, 12);
        const ph = Math.max((zMax - zMin) * scale, 12);

        const isHovered = this.hoveredItem && this.hoveredItem.type === 'door' && this.hoveredItem.index === idx;
        const isDragging = this.draggingItem && this.draggingItem.type === 'door' && this.draggingItem.index === idx;

        this.ctx.fillStyle = isDragging ? 'rgba(63, 185, 80, 0.9)' : (isHovered ? 'rgba(63, 185, 80, 0.75)' : 'rgba(63, 185, 80, 0.5)');
        this.ctx.strokeStyle = isDragging ? '#3fb950' : (isHovered ? '#ffffff' : '#2ea043');
        this.ctx.lineWidth = isHovered || isDragging ? 3.0 : 1.5;

        this.ctx.fillRect(px, py, pw, ph);
        this.ctx.strokeRect(px, py, pw, ph);

        const badgeText = `☰ ${dor.name.split(':')[1] || dor.name}`;
        this.ctx.font = 'bold 10px sans-serif';
        const textW = this.ctx.measureText(badgeText).width;

        const badgeX = px + pw * 0.5 - textW * 0.5 - 4;
        const badgeY = py - 6;

        this.ctx.fillStyle = isDragging ? '#238636' : (isHovered ? '#2ea043' : '#21262d');
        this.ctx.strokeStyle = '#30363d';
        this.ctx.lineWidth = 1;
        this.ctx.fillRect(badgeX, badgeY - 11, textW + 8, 14);
        this.ctx.strokeRect(badgeX, badgeY - 11, textW + 8, 14);

        this.ctx.fillStyle = '#ffffff';
        this.ctx.fillText(badgeText, badgeX + 4, badgeY);

        if (isHovered || isDragging) {
          this.ctx.fillStyle = '#e6edf3';
          this.ctx.font = '9px monospace';
          const posStr = (dor.name.includes('Partition')) ? `Z: ${(0.5 * (zMin + zMax)).toFixed(2)}m` : `X: ${(0.5 * (xMin + xMax)).toFixed(2)}m`;
          this.ctx.fillText(`↔ Drag (${posStr})`, badgeX + 4, badgeY + 13);
        }
      });
    }

    this.ctx.restore();
  }

  renderAxes(data, offsetX, offsetY, renderW, renderH) {
    this.ctx.save();
    this.ctx.strokeStyle = '#30363d';
    this.ctx.lineWidth = 1.5;
    this.ctx.strokeRect(offsetX, offsetY, renderW, renderH);

    // Coordinate Labels
    this.ctx.fillStyle = '#8b949e';
    this.ctx.font = '11px monospace';
    this.ctx.fillText(`0.0m`, offsetX - 15, offsetY + renderH + 15);
    this.ctx.fillText(`${data.l1.toFixed(1)}m (${data.dim1_name})`, offsetX + renderW - 30, offsetY + renderH + 15);
    this.ctx.fillText(`${data.l2.toFixed(1)}m (${data.dim2_name})`, offsetX - 35, offsetY + 12);

    // Center divider label for Room 1 / Room 2
    if (data.dim1_name === 'X') {
      const midX = offsetX + renderW * 0.5;
      this.ctx.fillStyle = 'rgba(88, 166, 255, 0.6)';
      this.ctx.fillText('Room 1 (West Suite)', offsetX + renderW * 0.15, offsetY + 18);
      this.ctx.fillText('Room 2 (East Suite)', midX + renderW * 0.15, offsetY + 18);
    }

    this.ctx.restore();
  }

  // Find if mouse is over an opening (window or door)
  findOpeningAtMouse(physX, physZ) {
    if (!this.sceneData) return null;

    const hitMargin = 0.45; // meters hit tolerance for easy clicking & dragging

    if (this.sceneData.windows) {
      for (let i = 0; i < this.sceneData.windows.length; ++i) {
        const w = this.sceneData.windows[i];
        const b = w.box;
        if (physX >= b[0] - hitMargin && physX <= b[1] + hitMargin &&
            physZ >= b[4] - hitMargin && physZ <= b[5] + hitMargin) {
          return { type: 'window', index: i, item: w };
        }
      }
    }

    if (this.sceneData.doors) {
      for (let i = 0; i < this.sceneData.doors.length; ++i) {
        const d = this.sceneData.doors[i];
        const b = d.box;
        if (physX >= b[0] - hitMargin && physX <= b[1] + hitMargin &&
            physZ >= b[4] - hitMargin && physZ <= b[5] + hitMargin) {
          return { type: 'door', index: i, item: d };
        }
      }
    }

    return null;
  }

  onMouseDown(e) {
    if (!this.currentSlice || !this.renderTransform || this.currentSlice.axis !== 1) return;

    const rect = this.canvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    const t = this.renderTransform;
    const normX = (mouseX - t.offsetX) / t.renderW;
    const normZ = 1.0 - (mouseY - t.offsetY) / t.renderH;
    const physX = normX * t.l1;
    const physZ = normZ * t.l2;

    const opening = this.findOpeningAtMouse(physX, physZ);
    if (opening) {
      this.draggingItem = {
        type: opening.type,
        index: opening.index,
        startX: physX,
        startZ: physZ,
        origBox: [...opening.item.box],
        currentX: 0.5 * (opening.item.box[0] + opening.item.box[1]),
        currentZ: 0.5 * (opening.item.box[4] + opening.item.box[5])
      };
      this.canvas.style.cursor = 'grabbing';
      e.preventDefault();
    }
  }

  onMouseMove(e) {
    if (!this.currentSlice || !this.renderTransform) return;

    const rect = this.canvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    const t = this.renderTransform;
    const normX = (mouseX - t.offsetX) / t.renderW;
    const normY = 1.0 - (mouseY - t.offsetY) / t.renderH;
    const phys1 = normX * t.l1;
    const phys2 = normY * t.l2;

    // Handle Active Dragging of Window / Door
    if (this.draggingItem && this.currentSlice.axis === 1) {
      const deltaX = phys1 - this.draggingItem.startX;
      const deltaZ = phys2 - this.draggingItem.startZ;

      const origCenterNormX = 0.5 * (this.draggingItem.origBox[0] + this.draggingItem.origBox[1]);
      const origCenterNormZ = 0.5 * (this.draggingItem.origBox[4] + this.draggingItem.origBox[5]);

      const targetX = origCenterNormX + deltaX;
      const targetZ = origCenterNormZ + deltaZ;

      this.draggingItem.currentX = targetX;
      this.draggingItem.currentZ = targetZ;

      // Update local preview box immediately for silky smooth visual feedback
      if (this.draggingItem.type === 'window' && this.sceneData.windows[this.draggingItem.index]) {
        const w = this.sceneData.windows[this.draggingItem.index];
        const halfW = 0.5 * (this.draggingItem.origBox[1] - this.draggingItem.origBox[0]);
        const halfD = 0.5 * (this.draggingItem.origBox[5] - this.draggingItem.origBox[4]);
        if (w.normal && Math.abs(w.normal.x) > 0.5) {
          const clampedZ = Math.max(halfD + 0.2, Math.min(5.0 - halfD - 0.2, targetZ));
          w.box[4] = clampedZ - halfD;
          w.box[5] = clampedZ + halfD;
        } else {
          const minX = (w.room_id === 1) ? (halfW + 0.2) : (5.0 + halfW + 0.2);
          const maxX = (w.room_id === 1) ? (5.0 - halfW - 0.2) : (10.0 - halfW - 0.2);
          const clampedX = Math.max(minX, Math.min(maxX, targetX));
          w.box[0] = clampedX - halfW;
          w.box[1] = clampedX + halfW;
        }
      } else if (this.draggingItem.type === 'door' && this.sceneData.doors[this.draggingItem.index]) {
        const d = this.sceneData.doors[this.draggingItem.index];
        const halfW = 0.5 * (this.draggingItem.origBox[1] - this.draggingItem.origBox[0]);
        const halfD = 0.5 * (this.draggingItem.origBox[5] - this.draggingItem.origBox[4]);
        if (d.name.includes('Partition')) {
          const clampedZ = Math.max(halfD + 0.3, Math.min(5.0 - halfD - 0.3, targetZ));
          d.box[4] = clampedZ - halfD;
          d.box[5] = clampedZ + halfD;
        } else {
          const clampedX = Math.max(5.0 + halfW + 0.3, Math.min(10.0 - halfW - 0.3, targetX));
          d.box[0] = clampedX - halfW;
          d.box[1] = clampedX + halfW;
        }
      }
      return;
    }

    // Check Hover on Window / Door
    if (this.currentSlice.axis === 1) {
      const opening = this.findOpeningAtMouse(phys1, phys2);
      if (opening) {
        this.hoveredItem = { type: opening.type, index: opening.index };
        this.canvas.style.cursor = 'grab';
      } else {
        this.hoveredItem = null;
        this.canvas.style.cursor = 'crosshair';
      }
    }

    if (mouseX < t.offsetX || mouseX > t.offsetX + t.renderW ||
        mouseY < t.offsetY || mouseY > t.offsetY + t.renderH) {
      const tip = document.getElementById('probe-tooltip');
      if (tip) tip.style.display = 'none';
      return;
    }

    const i = Math.min(t.n1 - 1, Math.max(0, Math.floor(normX * t.n1)));
    const j = Math.min(t.n2 - 1, Math.max(0, Math.floor(normY * t.n2)));
    const idx = j * t.n1 + i;

    const temp = this.currentSlice.temperature[idx];
    const u = this.currentSlice.u[idx];
    const v = this.currentSlice.v[idx];
    const speed = this.currentSlice.speed[idx];
    const p = this.currentSlice.pressure[idx];
    const type = this.currentSlice.cell_type[idx];

    let typeStr = "Air (Fluid)";
    if (type === 1) typeStr = "Solid Wall";
    if (type === 2) typeStr = "Inflow Window (Kerala Wind)";
    if (type === 3) typeStr = "Outflow Window";
    if (type === 4) typeStr = "Doorway";
    if (type === 5) typeStr = "Human Body (Hot Source @ 37°C)";

    // Update Floating Tooltip
    const tip = document.getElementById('probe-tooltip');
    if (tip && !this.draggingItem) {
      tip.style.display = 'block';
      tip.style.left = `${mouseX + 15}px`;
      tip.style.top = `${mouseY + 15}px`;

      let extraHint = "";
      if (this.hoveredItem) {
        extraHint = `<div style="color:#58a6ff;margin-top:4px;font-weight:bold;">👉 Click & Drag to reposition opening!</div>`;
      }

      tip.innerHTML = `
        <div class="tip-header">Point Probe (${this.currentSlice.dim1_name}: ${phys1.toFixed(2)}m, ${this.currentSlice.dim2_name}: ${phys2.toFixed(2)}m)</div>
        <div><b>Temperature:</b> <span style="color:#d29922;font-weight:700;">${temp.toFixed(2)} °C</span></div>
        <div><b>Velocity:</b> [u: ${u.toFixed(2)}, v: ${v.toFixed(2)}] m/s</div>
        <div><b>Speed:</b> <span style="color:#39c5cf;font-weight:700;">${speed.toFixed(2)} m/s</span></div>
        <div><b>Pressure:</b> ${p.toFixed(3)} Pa</div>
        <div style="font-size:0.72rem;color:#8b949e;margin-top:2px;"><b>Region:</b> ${typeStr}</div>
        ${extraHint}
      `;
    }

    if (window.app && window.app.updateSidebarProbe) {
      window.app.updateSidebarProbe({
        x: phys1, y: (this.currentSlice.axis === 1 ? this.currentSlice.coord_val : phys2),
        z: (this.currentSlice.axis === 2 ? this.currentSlice.coord_val : (this.currentSlice.axis === 0 ? phys1 : phys2)),
        temperature: temp,
        u: u, v: v, w: 0.0, speed: speed, pressure: p, cell_type: type
      });
    }
  }

  onMouseUp(e) {
    if (this.draggingItem) {
      const type = this.draggingItem.type;
      const idx = this.draggingItem.index;
      const targetX = this.draggingItem.currentX;
      const targetZ = this.draggingItem.currentZ;

      this.draggingItem = null;
      this.canvas.style.cursor = 'default';

      // Fire move event to backend
      if (window.app && window.app.onOpeningMoved) {
        window.app.onOpeningMoved(type, idx, targetX, targetZ);
      }
    }
  }
}
