// Scientific Colormaps for 2D/3D CFD Thermal and Velocity Fields

const Colormaps = {
  // Turbo Colormap polynomial coefficients (Google AI Research)
  turbo: function(t) {
    t = Math.max(0.0, Math.min(1.0, t));
    const kRedVec4 = [0.13572138, 4.61539260, -42.66032258, 132.13108234];
    const kGreenVec4 = [0.09140261, 2.19418839, 4.84296658, -14.18503333];
    const kBlueVec4 = [0.1079832, 2.7711209, -27.241945, 96.932997];
    const kRedVec2 = [-152.94239396, 59.28637943];
    const kGreenVec2 = [4.27729857, 2.82956604];
    const kBlueVec2 = [-141.77703, 58.748364];

    const v4 = [1.0, t, t * t, t * t * t];
    const v2 = [v4[3] * t, v4[3] * t * t];

    let r = kRedVec4[0]*v4[0] + kRedVec4[1]*v4[1] + kRedVec4[2]*v4[2] + kRedVec4[3]*v4[3] + kRedVec2[0]*v2[0] + kRedVec2[1]*v2[1];
    let g = kGreenVec4[0]*v4[0] + kGreenVec4[1]*v4[1] + kGreenVec4[2]*v4[2] + kGreenVec4[3]*v4[3] + kGreenVec2[0]*v2[0] + kGreenVec2[1]*v2[1];
    let b = kBlueVec4[0]*v4[0] + kBlueVec4[1]*v4[1] + kBlueVec4[2]*v4[2] + kBlueVec4[3]*v4[3] + kBlueVec2[0]*v2[0] + kBlueVec2[1]*v2[1];

    return [
      Math.round(Math.max(0, Math.min(255, r * 255))),
      Math.round(Math.max(0, Math.min(255, g * 255))),
      Math.round(Math.max(0, Math.min(255, b * 255)))
    ];
  },

  // Classic Jet Colormap
  jet: function(t) {
    t = Math.max(0.0, Math.min(1.0, t));
    let r = Math.max(0, Math.min(1, 1.5 - Math.abs(t * 4 - 3)));
    let g = Math.max(0, Math.min(1, 1.5 - Math.abs(t * 4 - 2)));
    let b = Math.max(0, Math.min(1, 1.5 - Math.abs(t * 4 - 1)));
    return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
  },

  // Viridis Colormap (Perceptually Uniform)
  viridis: function(t) {
    t = Math.max(0.0, Math.min(1.0, t));
    const c0 = [68, 1, 84];
    const c1 = [59, 82, 139];
    const c2 = [33, 145, 140];
    const c3 = [94, 201, 98];
    const c4 = [253, 231, 37];

    if (t < 0.25) {
      let f = t / 0.25;
      return [Math.round(c0[0]*(1-f)+c1[0]*f), Math.round(c0[1]*(1-f)+c1[1]*f), Math.round(c0[2]*(1-f)+c1[2]*f)];
    } else if (t < 0.5) {
      let f = (t - 0.25) / 0.25;
      return [Math.round(c1[0]*(1-f)+c2[0]*f), Math.round(c1[1]*(1-f)+c2[1]*f), Math.round(c1[2]*(1-f)+c2[2]*f)];
    } else if (t < 0.75) {
      let f = (t - 0.5) / 0.25;
      return [Math.round(c2[0]*(1-f)+c3[0]*f), Math.round(c2[1]*(1-f)+c3[1]*f), Math.round(c2[2]*(1-f)+c3[2]*f)];
    } else {
      let f = (t - 0.75) / 0.25;
      return [Math.round(c3[0]*(1-f)+c4[0]*f), Math.round(c3[1]*(1-f)+c4[1]*f), Math.round(c3[2]*(1-f)+c4[2]*f)];
    }
  },

  // Inferno Thermal Colormap
  inferno: function(t) {
    t = Math.max(0.0, Math.min(1.0, t));
    const c0 = [0, 0, 4];
    const c1 = [87, 16, 110];
    const c2 = [187, 55, 84];
    const c3 = [249, 142, 9];
    const c4 = [252, 255, 164];

    if (t < 0.25) {
      let f = t / 0.25;
      return [Math.round(c0[0]*(1-f)+c1[0]*f), Math.round(c0[1]*(1-f)+c1[1]*f), Math.round(c0[2]*(1-f)+c1[2]*f)];
    } else if (t < 0.5) {
      let f = (t - 0.25) / 0.25;
      return [Math.round(c1[0]*(1-f)+c2[0]*f), Math.round(c1[1]*(1-f)+c2[1]*f), Math.round(c1[2]*(1-f)+c2[2]*f)];
    } else if (t < 0.75) {
      let f = (t - 0.5) / 0.25;
      return [Math.round(c2[0]*(1-f)+c3[0]*f), Math.round(c2[1]*(1-f)+c3[1]*f), Math.round(c2[2]*(1-f)+c3[2]*f)];
    } else {
      let f = (t - 0.75) / 0.25;
      return [Math.round(c3[0]*(1-f)+c4[0]*f), Math.round(c3[1]*(1-f)+c4[1]*f), Math.round(c3[2]*(1-f)+c4[2]*f)];
    }
  },

  // CoolWarm Diverging Colormap
  coolwarm: function(t) {
    t = Math.max(0.0, Math.min(1.0, t));
    let r = (t < 0.5) ? (59 + 2 * t * (220 - 59)) : (220 + 2 * (t - 0.5) * (180 - 220));
    let g = (t < 0.5) ? (76 + 2 * t * (220 - 76)) : (220 + 2 * (t - 0.5) * (4 - 220));
    let b = (t < 0.5) ? (192 + 2 * t * (220 - 192)) : (220 + 2 * (t - 0.5) * (38 - 220));
    return [Math.round(r), Math.round(g), Math.round(b)];
  },

  // Normalized color lookup
  getColor: function(name, val, minVal, maxVal) {
    let t = val;
    if (minVal !== undefined && maxVal !== undefined && maxVal > minVal) {
      t = (val - minVal) / (maxVal - minVal);
    }
    t = Math.max(0.0, Math.min(1.0, isNaN(t) ? 0.0 : t));
    const fn = this[name] || this.turbo;
    return fn(t);
  },

  sample: function(name, val, minVal, maxVal) {
    return this.getColor(name, val, minVal, maxVal);
  },

  getCSSColor: function(name, val, minVal, maxVal) {
    const rgb = this.getColor(name, val, minVal, maxVal);
    return `rgb(${rgb[0]}, ${rgb[1]}, ${rgb[2]})`;
  }
};
