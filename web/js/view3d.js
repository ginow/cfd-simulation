// 3D Volumetric Room & Flow Visualizer

class View3D {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.roomGroup = null;
    this.particleGroup = null;
    this.sliceMesh = null;
    this.occupantMeshes = [];
    this.particles = [];
    this.colormapName = 'turbo';

    this.isOrbiting = false;
    this.isPanning = false;
    this.prevMouseX = 0;
    this.prevMouseY = 0;
    this.cameraTarget = { x: 5.0, y: 1.5, z: 2.5 };
    this.cameraDistance = 14.0;
    this.cameraPhi = Math.PI / 4;
    this.cameraTheta = Math.PI / 5;

    this.init();
  }

  init() {
    // If THREE is loaded, setup scene
    if (typeof THREE === 'undefined') {
      console.warn("Three.js not yet loaded, waiting...");
      return;
    }

    const w = this.container.clientWidth || 800;
    const h = this.container.clientHeight || 600;

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x05070a);

    this.camera = new THREE.PerspectiveCamera(45, w / h, 0.1, 100);
    this.updateCameraPos();

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(w, h);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.container.appendChild(this.renderer.domElement);

    // Lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.7);
    this.scene.add(ambientLight);

    const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
    dirLight.position.set(10, 15, 10);
    this.scene.add(dirLight);

    const floorGrid = new THREE.GridHelper(12, 24, 0x30363d, 0x161b22);
    floorGrid.position.set(5.0, 0.0, 2.5);
    this.scene.add(floorGrid);

    this.roomGroup = new THREE.Group();
    this.scene.add(this.roomGroup);

    this.particleGroup = new THREE.Group();
    this.scene.add(this.particleGroup);

    this.setupInteraction();
    window.addEventListener('resize', () => this.resize());
  }

  updateCameraPos() {
    this.camera.position.x = this.cameraTarget.x + this.cameraDistance * Math.sin(this.cameraTheta) * Math.cos(this.cameraPhi);
    this.camera.position.y = this.cameraTarget.y + this.cameraDistance * Math.sin(this.cameraPhi);
    this.camera.position.z = this.cameraTarget.z + this.cameraDistance * Math.cos(this.cameraTheta) * Math.cos(this.cameraPhi);
    this.camera.lookAt(this.cameraTarget.x, this.cameraTarget.y, this.cameraTarget.z);
  }

  setupInteraction() {
    const el = this.renderer ? this.renderer.domElement : this.container;

    el.addEventListener('mousedown', (e) => {
      if (e.button === 0) this.isOrbiting = true;
      if (e.button === 2) this.isPanning = true;
      this.prevMouseX = e.clientX;
      this.prevMouseY = e.clientY;
    });

    window.addEventListener('mousemove', (e) => {
      if (!this.isOrbiting && !this.isPanning) return;
      const dx = e.clientX - this.prevMouseX;
      const dy = e.clientY - this.prevMouseY;
      this.prevMouseX = e.clientX;
      this.prevMouseY = e.clientY;

      if (this.isOrbiting) {
        this.cameraTheta -= dx * 0.008;
        this.cameraPhi = Math.max(0.05, Math.min(Math.PI / 2 - 0.05, this.cameraPhi + dy * 0.008));
        this.updateCameraPos();
      } else if (this.isPanning) {
        this.cameraTarget.x -= dx * 0.01;
        this.cameraTarget.y += dy * 0.01;
        this.updateCameraPos();
      }
    });

    window.addEventListener('mouseup', () => {
      this.isOrbiting = false;
      this.isPanning = false;
    });

    el.addEventListener('wheel', (e) => {
      e.preventDefault();
      this.cameraDistance = Math.max(4.0, Math.min(30.0, this.cameraDistance + e.deltaY * 0.015));
      this.updateCameraPos();
    }, { passive: false });

    el.addEventListener('contextmenu', (e) => e.preventDefault());
  }

  resize() {
    if (!this.renderer || !this.camera) return;
    const w = this.container.clientWidth;
    const h = this.container.clientHeight;
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(w, h);
  }

  setupRooms(data) {
    if (!this.scene) this.init();
    if (!this.scene) return;

    // Clear previous geometry
    while (this.roomGroup.children.length > 0) {
      this.roomGroup.remove(this.roomGroup.children[0]);
    }

    const wireMat = new THREE.LineBasicMaterial({ color: 0x58a6ff, transparent: true, opacity: 0.4 });
    const wallMat = new THREE.MeshStandardMaterial({
      color: 0x21262d,
      transparent: true,
      opacity: 0.18,
      side: THREE.DoubleSide
    });

    // 1. Room Outer Bounds Wireframe
    const boxGeo = new THREE.BoxGeometry(data.lx, data.ly, data.lz);
    const edges = new THREE.EdgesGeometry(boxGeo);
    const wireframe = new THREE.LineSegments(edges, wireMat);
    wireframe.position.set(data.lx * 0.5, data.ly * 0.5, data.lz * 0.5);
    this.roomGroup.add(wireframe);

    // 2. Central Partition Wall
    const midWallGeo = new THREE.BoxGeometry(0.1, data.ly, data.lz);
    const midWall = new THREE.Mesh(midWallGeo, wallMat);
    midWall.position.set(data.lx * 0.5, data.ly * 0.5, data.lz * 0.5);
    this.roomGroup.add(midWall);

    // 3. Add Windows
    for (let w of (data.windows || [])) {
      const b = w.box;
      const ww = Math.max(0.1, b[1] - b[0]);
      const wh = Math.max(0.1, b[3] - b[2]);
      const wd = Math.max(0.1, b[5] - b[4]);

      const winColor = w.is_inflow ? 0x39c5cf : 0xd29922; // Cyan for Kerala Inflow, Orange for Outflow
      const winMat = new THREE.MeshStandardMaterial({
        color: winColor,
        transparent: true,
        opacity: 0.75,
        roughness: 0.2
      });

      const winMesh = new THREE.Mesh(new THREE.BoxGeometry(ww, wh, wd), winMat);
      winMesh.position.set((b[0] + b[1]) * 0.5, (b[2] + b[3]) * 0.5, (b[4] + b[5]) * 0.5);
      this.roomGroup.add(winMesh);
    }

    // 4. Add Doors
    for (let d of (data.doors || [])) {
      const b = d.box;
      const dw = Math.max(0.1, b[1] - b[0]);
      const dh = Math.max(0.1, b[3] - b[2]);
      const dd = Math.max(0.1, b[5] - b[4]);

      const doorMat = new THREE.MeshStandardMaterial({
        color: 0x3fb950,
        transparent: true,
        opacity: 0.55
      });

      const doorMesh = new THREE.Mesh(new THREE.BoxGeometry(dw, dh, dd), doorMat);
      doorMesh.position.set((b[0] + b[1]) * 0.5, (b[2] + b[3]) * 0.5, (b[4] + b[5]) * 0.5);
      this.roomGroup.add(doorMesh);
    }

    // 5. Add Human Occupants (Hot Bodies @ 37°C)
    for (let occ of (data.occupants || [])) {
      const b = occ.box;
      const occGroup = new THREE.Group();

      // Body torso
      const bodyMat = new THREE.MeshStandardMaterial({
        color: 0xf85149,
        emissive: 0x7a1e1e,
        roughness: 0.3
      });

      const torso = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.2, 1.0, 16), bodyMat);
      torso.position.set(0, 0.9, 0);
      occGroup.add(torso);

      // Head
      const head = new THREE.Mesh(new THREE.SphereGeometry(0.16, 16, 16), bodyMat);
      head.position.set(0, 1.55, 0);
      occGroup.add(head);

      // Position in room
      const cx = (b[0] + b[1]) * 0.5;
      const cz = (b[4] + b[5]) * 0.5;
      occGroup.position.set(cx, 0, cz);

      this.roomGroup.add(occGroup);
    }
  }

  update3DState(data) {
    if (!this.scene) return;

    // Refresh room geometry and aperture meshes if data changes
    this.setupRooms(data);

    // Update 3D Flow Particles / Streamlines
    while (this.particleGroup.children.length > 0) {
      this.particleGroup.remove(this.particleGroup.children[0]);
    }

    if (data.particles && data.particles.length > 0) {
      const geom = new THREE.BufferGeometry();
      const positions = [];
      const colors = [];

      for (let p of data.particles) {
        // [x, y, z, u, v, w, t]
        const px = p[0], py = p[1], pz = p[2];
        const u = p[3], v = p[4], w = p[5];
        const temp = p[6];

        const speed = Math.sqrt(u * u + v * v + w * w);
        const len = Math.min(0.6, speed * 0.3);

        if (speed > 0.01) {
          positions.push(px, py, pz);
          positions.push(px + (u / speed) * len, py + (v / speed) * len, pz + (w / speed) * len);

          const normT = Math.min(1.0, Math.max(0.0, (temp - 32.0) / 5.0));
          const rgb = Colormaps.getColor(this.colormapName, normT);
          const r = rgb[0] / 255, g = rgb[1] / 255, b = rgb[2] / 255;

          colors.push(r, g, b);
          colors.push(r, g, b);
        }
      }

      geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
      geom.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));

      const lineMat = new THREE.LineBasicMaterial({
        vertexColors: true,
        linewidth: 2,
        transparent: true,
        opacity: 0.85
      });

      const lineSegments = new THREE.LineSegments(geom, lineMat);
      this.particleGroup.add(lineSegments);
    }
  }

  render() {
    if (!this.renderer || !this.scene || !this.camera) return;
    this.renderer.render(this.scene, this.camera);
  }
}
