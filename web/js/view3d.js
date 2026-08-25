// 3D Volumetric Scene & Coupled CFD Convection Visualizer

class View3D {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.scene = null;
    this.camera = null;
    this.renderer = null;
    this.roomGroup = null;
    this.particleGroup = null;
    this.colormapName = 'turbo';

    this.isOrbiting = false;
    this.isPanning = false;
    this.prevMouseX = 0;
    this.prevMouseY = 0;
    this.cameraTarget = { x: 5.0, y: 1.4, z: 2.5 };
    this.cameraDistance = 14.0;
    this.cameraPhi = Math.PI / 4;
    this.cameraTheta = Math.PI / 5;

    this.init();
  }

  init() {
    if (typeof THREE === 'undefined') {
      console.warn("Three.js not yet loaded");
      return;
    }

    const w = this.container.clientWidth || 800;
    const h = this.container.clientHeight || 600;

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x05070a);

    this.camera = new THREE.PerspectiveCamera(45, w / h, 0.1, 100);
    this.updateCameraPos();

    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    this.renderer.setSize(w, h);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.container.appendChild(this.renderer.domElement);

    // Lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.75);
    this.scene.add(ambientLight);

    const dirLight = new THREE.DirectionalLight(0xffffff, 0.85);
    dirLight.position.set(12, 18, 12);
    this.scene.add(dirLight);

    const floorGrid = new THREE.GridHelper(14, 28, 0x30363d, 0x161b22);
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
    if (!this.camera) return;
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
        this.cameraTarget.z -= dy * 0.01;
        this.updateCameraPos();
      }
    });

    window.addEventListener('mouseup', () => {
      this.isOrbiting = false;
      this.isPanning = false;
    });

    el.addEventListener('wheel', (e) => {
      e.preventDefault();
      this.cameraDistance = Math.max(3.0, Math.min(30.0, this.cameraDistance + e.deltaY * 0.015));
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

  update3DState(sceneData) {
    if (!this.scene || !sceneData) return;

    // Clear old geometry
    while (this.roomGroup.children.length > 0) {
      const obj = this.roomGroup.children[0];
      if (obj.geometry) obj.geometry.dispose();
      this.roomGroup.remove(obj);
    }

    const wallMat = new THREE.MeshStandardMaterial({
      color: 0x30363d,
      transparent: true,
      opacity: 0.35,
      roughness: 0.6,
      side: THREE.DoubleSide
    });

    const frameMat = new THREE.LineBasicMaterial({ color: 0x58a6ff, linewidth: 2 });

    // 1. Build 3D Walls (Extruded to 2.80 m Fixed Ceiling Height)
    const walls = sceneData.walls || [];
    for (const w of walls) {
      const dx = w.x2 - w.x1;
      const dz = w.z2 - w.z1;
      const len = Math.sqrt(dx * dx + dz * dz);
      if (len < 0.05) continue;

      const h = w.height || 2.80; // Fixed 2.80m
      const th = w.thickness || 0.20;

      const geom = new THREE.BoxGeometry(len, h, th);
      const mesh = new THREE.Mesh(geom, wallMat);

      const mx = (w.x1 + w.x2) * 0.5;
      const mz = (w.z1 + w.z2) * 0.5;
      mesh.position.set(mx, h * 0.5, mz);
      mesh.rotation.y = -Math.atan2(dz, dx);

      this.roomGroup.add(mesh);

      // Wireframe Outline
      const wireGeom = new THREE.EdgesGeometry(geom);
      const wire = new THREE.LineSegments(wireGeom, frameMat);
      wire.position.copy(mesh.position);
      wire.rotation.copy(mesh.rotation);
      this.roomGroup.add(wire);
    }

    // 2. Build 3D Windows (with Rotated Casement Glass Panes)
    const windows = sceneData.windows || [];
    for (const win of windows) {
      const w = win.width || 1.50;
      const h = win.height || 1.20;
      const openRatio = (win.open_ratio !== undefined) ? win.open_ratio : 1.0;
      const openAngle = openRatio * (Math.PI / 2);

      // Window Frame Outline
      const frameGeom = new THREE.BoxGeometry(w, h, 0.25);
      const frameEdge = new THREE.EdgesGeometry(frameGeom);
      const frameMesh = new THREE.LineSegments(frameEdge, new THREE.LineBasicMaterial({ color: 0x39c5cf, linewidth: 2 }));
      frameMesh.position.set(win.x, win.y || 1.50, win.z);
      this.roomGroup.add(frameMesh);

      // Rotated Casement Sash Pane
      const glassMat = new THREE.MeshStandardMaterial({
        color: 0x39c5cf,
        transparent: true,
        opacity: 0.65,
        roughness: 0.1,
        metalness: 0.8,
        side: THREE.DoubleSide
      });

      const paneGeom = new THREE.BoxGeometry(w * 0.95, h * 0.95, 0.04);
      paneGeom.translate(w * 0.45, 0, 0); // Pivot at hinge edge
      const paneMesh = new THREE.Mesh(paneGeom, glassMat);
      paneMesh.position.set(win.x - w * 0.45, win.y || 1.50, win.z);
      paneMesh.rotation.y = openAngle;
      this.roomGroup.add(paneMesh);
    }

    // 3. Build 3D Doors (with Rotated Wooden Door Leaf)
    const doors = sceneData.doors || [];
    for (const dor of doors) {
      const w = dor.width || 1.00;
      const h = dor.height || 2.10;
      const openRatio = (dor.open_ratio !== undefined) ? dor.open_ratio : 1.0;
      const openAngle = openRatio * (Math.PI / 2);

      // Door Frame
      const frameGeom = new THREE.BoxGeometry(w, h, 0.25);
      const frameEdge = new THREE.EdgesGeometry(frameGeom);
      const frameMesh = new THREE.LineSegments(frameEdge, new THREE.LineBasicMaterial({ color: 0xbc8cff, linewidth: 2 }));
      frameMesh.position.set(dor.x, h * 0.5, dor.z);
      this.roomGroup.add(frameMesh);

      // Rotated Door Leaf
      const doorMat = new THREE.MeshStandardMaterial({
        color: 0x8b5cf6,
        roughness: 0.4,
        side: THREE.DoubleSide
      });
      const leafGeom = new THREE.BoxGeometry(w * 0.95, h * 0.95, 0.05);
      leafGeom.translate(w * 0.45, 0, 0); // Pivot at hinge
      const leafMesh = new THREE.Mesh(leafGeom, doorMat);
      leafMesh.position.set(dor.x - w * 0.45, h * 0.5, dor.z);
      leafMesh.rotation.y = openAngle;
      this.roomGroup.add(leafMesh);
    }

    // 4. Build 6-Foot Human Mannequins (Height = 1.83 m @ 37.0 °C)
    const occupants = sceneData.occupants || [];
    for (const occ of occupants) {
      const hTotal = occ.height || 1.83; // 6 feet
      const occMat = new THREE.MeshStandardMaterial({
        color: 0xf85149,
        emissive: 0xaa2211,
        roughness: 0.3
      });

      // Torso Cylinder (1.4m height)
      const torsoGeom = new THREE.CylinderGeometry(0.20, 0.16, hTotal * 0.75, 16);
      const torso = new THREE.Mesh(torsoGeom, occMat);
      torso.position.set(occ.x, hTotal * 0.38, occ.z);
      this.roomGroup.add(torso);

      // Head Sphere (at top of 6ft height)
      const headGeom = new THREE.SphereGeometry(0.13, 16, 16);
      const head = new THREE.Mesh(headGeom, occMat);
      head.position.set(occ.x, hTotal * 0.88, occ.z);
      this.roomGroup.add(head);
    }

    // 5. Build 3D Streamline Flow Particles
    while (this.particleGroup.children.length > 0) {
      const p = this.particleGroup.children[0];
      if (p.geometry) p.geometry.dispose();
      this.particleGroup.remove(p);
    }

    const particles = sceneData.particles || [];
    if (particles.length > 0) {
      const posArr = new Float32Array(particles.length * 3);
      const colArr = new Float32Array(particles.length * 3);

      for (let i = 0; i < particles.length; ++i) {
        const pt = particles[i]; // [x, y, z, u, v, w, t]
        posArr[i * 3] = pt[0];
        posArr[i * 3 + 1] = pt[1];
        posArr[i * 3 + 2] = pt[2];

        const temp = pt[6];
        const minT = this.minTemp || 30.0;
        const maxT = this.maxTemp || 37.0;
        const rgb = Colormaps.getColor(this.colormapName, temp, minT, maxT);
        colArr[i * 3] = rgb[0] / 255;
        colArr[i * 3 + 1] = rgb[1] / 255;
        colArr[i * 3 + 2] = rgb[2] / 255;
      }

      const pGeom = new THREE.BufferGeometry();
      pGeom.setAttribute('position', new THREE.BufferAttribute(posArr, 3));
      pGeom.setAttribute('color', new THREE.BufferAttribute(colArr, 3));

      const pMat = new THREE.PointsMaterial({
        size: 0.12,
        vertexColors: true,
        transparent: true,
        opacity: 0.9
      });

      const pSys = new THREE.Points(pGeom, pMat);
      this.particleGroup.add(pSys);
    }
  }

  render() {
    if (!this.renderer || !this.scene || !this.camera) return;
    this.renderer.render(this.scene, this.camera);
  }
}
