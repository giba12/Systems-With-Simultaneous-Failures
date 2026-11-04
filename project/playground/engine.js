/* engine.js - a small JavaScript port of the C++ library, for systems of up to 6 components.
   Same maths as the project: level sweep (Lemma 1), tail signature, signature, MTTF (Eq. 7),
   Levy-frailty kernel (Eq. 20) with the non-negative increment recursion, repair policies by
   enumeration of sub-states, and the optimal policy by Dinkelbach's method. */

const BINOM = [];
for (let a = 0; a <= 12; a++) {
  BINOM[a] = [];
  for (let b = 0; b <= a; b++) BINOM[a][b] = b === 0 || b === a ? 1 : BINOM[a - 1][b - 1] + BINOM[a - 1][b];
}
const choose = (a, b) => (b < 0 || b > a ? 0 : BINOM[a][b]);
const pop = (x) => { let c = 0; while (x) { c += x & 1; x >>= 1; } return c; };

/* ---------- systems ---------- */
function makeSystem(id, name, labels, colors, phi, blurb) {
  const n = labels.length;
  return { id, name, labels, colors, n, phi, blurb, all: (1 << n) - 1 };
}
const SYSTEMS = [
  makeSystem('sat', 'Satellite', ['P1', 'P2', 'C1', 'C2', 'K1', 'K2'],
    ['power', 'power', 'comms', 'comms', 'comp', 'comp'],
    (x) => (x & 3) !== 0 && (x & 12) !== 0 && (x & 48) !== 0,
    'Three pairs: power, communications, computers. Each pair needs one working unit.'),
  makeSystem('fig1', 'Paper Fig. 1', ['x1', 'x2', 'x3'], ['comms', 'comms', 'power'],
    (x) => ((x & 4) !== 0) || ((x & 3) === 3),
    'Works if x3 works, or if x1 and x2 both work.'),
  makeSystem('two4', 'Two of four', ['A', 'B', 'C', 'D'], ['comms', 'comms', 'comms', 'comms'],
    (x) => pop(x) >= 2,
    'Works while at least two of the four units work.'),
  makeSystem('ser4', 'Series of four', ['A', 'B', 'C', 'D'], ['comp', 'comp', 'comp', 'comp'],
    (x) => x === 15,
    'Every unit is needed.'),
  makeSystem('par3', 'Parallel of three', ['A', 'B', 'C'], ['power', 'power', 'power'],
    (x) => x !== 0,
    'Any one working unit is enough.'),
];
function seriesOf(n) {
  return makeSystem('series', 'Series', Array.from({ length: n }, (_, i) => 'S' + (i + 1)),
    Array(n).fill('comp'), (x) => x === (1 << n) - 1, '');
}

/* ---------- failure models ---------- */
function makeModel(type, beta) {
  if (type === 'exp') {
    return { type, name: 'Independent', nu: (w) => w, P: (w1, w2) => (w2 === w1 - 1 ? 1 : 0), inc: (w, k) => 1 / (w - k) };
  }
  const psi = type === 'lsh'
    ? (x) => (x <= 0 ? 0 : (1 - beta) * x + beta)
    : (x) => (x <= 0 ? 0 : (1 - Math.pow(1 - beta, x)) / beta);
  const N = 8;
  const psiV = Array.from({ length: N + 1 }, (_, i) => psi(i));
  const K = Array.from({ length: N + 1 }, () => Array(N + 1).fill(0));
  for (let w1 = 1; w1 <= N; w1++) {
    for (let w2 = 0; w2 < w1; w2++) {
      let sum = 0;
      for (let w = w2; w <= w1; w++) {
        const sign = (w - w2 + 1) % 2 === 0 ? 1 : -1;
        sum += sign * choose(w1 - w2, w1 - w) * psiV[w] / psiV[w1];
      }
      K[w1][w2] = Math.max(0, choose(w1, w1 - w2) * sum);
    }
  }
  const INC = Array.from({ length: N + 1 }, () => Array(N + 1).fill(0));
  for (let w = 1; w <= N; w++) {
    const reach = Array(w + 1).fill(0);
    reach[w] = 1;
    for (let v = w - 1; v >= 0; v--) {
      let t = 0;
      for (let a = v + 1; a <= w; a++) t += reach[a] * K[a][v];
      reach[v] = t;
    }
    for (let k = 0; k < w; k++) INC[w][k] = reach[w - k] / psiV[w - k];
  }
  return {
    type, name: type === 'lsh' ? 'Lethal shock' : 'Binomial failure rate',
    nu: (w) => psiV[w], P: (w1, w2) => (w2 >= w1 || w1 === 0 ? 0 : K[w1][w2]), inc: (w, k) => INC[w][k],
  };
}

/* ---------- the level sweep (Algorithm 1) ---------- */
function analyse(sys, model) {
  const n = sys.n, size = 1 << n;
  const W = new Array(size);
  const levels = Array.from({ length: n + 1 }, () => []);
  for (let x = 0; x < size; x++) levels[pop(x)].push(x);
  let phiCalls = 0;
  const order = [];
  for (let w = 0; w <= n; w++) {
    for (const x of levels[w]) {
      phiCalls++;
      const vec = [sys.phi(x) ? 1 : 0];
      for (let k = 1; k <= w; k++) {
        let total = 0;
        for (let i = 0; i < n; i++) if ((x >> i) & 1) total += W[x & ~(1 << i)][k - 1];
        if (total % k !== 0) throw new Error('not divisible');
        vec[k] = total / k;
      }
      W[x] = vec;
      order.push(x);
    }
  }
  const tail = new Array(size), sig = new Array(size), mttf = new Array(size);
  for (let x = 0; x < size; x++) {
    const w = pop(x), vec = W[x];
    if (vec[0] === 0) { tail[x] = vec.map(() => 0); sig[x] = []; mttf[x] = 0; continue; }
    tail[x] = vec.map((v, k) => v / choose(w, k));
    sig[x] = [];
    for (let k = 1; k <= w; k++) sig[x][k - 1] = tail[x][k - 1] - tail[x][k];
    let m = 0;
    for (let k = 0; k < w; k++) m += tail[x][k] * model.inc(w, k);
    mttf[x] = m;
  }
  const minimal = [];
  for (let x = 1; x < size; x++) if (W[x][0] === 1 && W[x][1] === 0) minimal.push(x);
  let peak = 0;
  for (let w = 0; w <= n; w++) peak = Math.max(peak, levels[w].length + (w > 0 ? levels[w - 1].length : 0));
  return { sys, model, W, tail, sig, mttf, levels, order, minimal, phiCalls, peak };
}

/* ---------- repair policies ---------- */
function repairCost(sys, costs, x) {
  return costs.set + (sys.n - pop(x)) * costs.comp + (sys.phi(x) ? 0 : costs.sys);
}
function evalPolicy(sys, model, rep, costs) {
  const size = 1 << sys.n, len = new Array(size), cost = new Array(size);
  for (let x = 0; x < size; x++) {
    if (rep[x]) { len[x] = 0; cost[x] = repairCost(sys, costs, x); continue; }
    const w = pop(x);
    let l = 1 / model.nu(w), c = 0;
    for (let y = (x - 1) & x; ; y = (y - 1) & x) {
      const f = model.P(w, pop(y)) / choose(w, w - pop(y));
      l += f * len[y]; c += f * cost[y];
      if (y === 0) break;
    }
    len[x] = l; cost[x] = c;
  }
  const all = sys.all;
  return { length: len[all], cost: cost[all], ltmc: cost[all] / len[all] };
}
function correctivePolicy(sys) {
  const rep = new Uint8Array(1 << sys.n);
  for (let x = 0; x < rep.length; x++) rep[x] = sys.phi(x) ? 0 : 1;
  return rep;
}
function rOutOfN(sys, r) {
  const rep = new Uint8Array(1 << sys.n);
  for (let x = 0; x < rep.length; x++) rep[x] = (!sys.phi(x) || (x !== sys.all && sys.n - pop(x) >= r)) ? 1 : 0;
  return rep;
}
function thresholdPolicy(sys, mttf, gamma) {
  const rep = new Uint8Array(1 << sys.n), top = mttf[sys.all];
  for (let x = 0; x < rep.length; x++) rep[x] = (!sys.phi(x) || (x !== sys.all && mttf[x] / top < gamma)) ? 1 : 0;
  return rep;
}
const countRepairs = (rep) => rep.reduce((a, b) => a + b, 0);

function thresholdCurve(sys, model, mttf, costs) {
  const top = mttf[sys.all];
  const ratios = [];
  for (let x = 0; x < mttf.length; x++) if (sys.phi(x) && x !== sys.all) ratios.push(mttf[x] / top);
  ratios.sort((a, b) => a - b);
  const distinct = [];
  for (const r of ratios) if (!distinct.length || r - distinct[distinct.length - 1] > 1e-9) distinct.push(r);
  const out = [];
  const make = (cut) => {
    const rep = new Uint8Array(1 << sys.n);
    for (let x = 0; x < rep.length; x++) {
      rep[x] = (!sys.phi(x) || (x !== sys.all && mttf[x] / top <= cut + 1e-9)) ? 1 : 0;
    }
    return rep;
  };
  out.push({ from: 0, to: distinct.length ? distinct[0] : 1, rep: make(-1) });
  for (let j = 0; j < distinct.length; j++) {
    out.push({ from: distinct[j], to: j + 1 < distinct.length ? distinct[j + 1] : 1, rep: make(distinct[j]) });
  }
  const pts = out.filter((p) => p.to > p.from + 1e-12 || p === out[0]);
  for (const p of pts) { p.ev = evalPolicy(sys, model, p.rep, costs); p.count = countRepairs(p.rep); }
  let best = 0;
  pts.forEach((p, i) => { if (p.ev.ltmc < pts[best].ev.ltmc - 1e-12) best = i; });
  return { points: pts, best };
}

function optimalPolicy(sys, model, costs) {
  const size = 1 << sys.n;
  let rep = correctivePolicy(sys);
  let g = evalPolicy(sys, model, rep, costs).ltmc;
  const rounds = [{ rent: null, count: countRepairs(rep), ltmc: g }];
  for (let guard = 0; guard < 60; guard++) {
    const h = new Array(size), plan = new Uint8Array(size);
    for (let x = 0; x < size; x++) {
      const w = pop(x), cx = repairCost(sys, costs, x);
      if (!sys.phi(x)) { h[x] = cx; plan[x] = 1; continue; }
      let cont = -g / model.nu(w);
      for (let y = (x - 1) & x; ; y = (y - 1) & x) {
        cont += model.P(w, pop(y)) / choose(w, w - pop(y)) * h[y];
        if (y === 0) break;
      }
      if (x === sys.all) { h[x] = cont; plan[x] = 0; continue; }
      if (cx < cont - 1e-12 * Math.max(1, Math.abs(cont))) { h[x] = cx; plan[x] = 1; } else { h[x] = cont; plan[x] = 0; }
    }
    const next = evalPolicy(sys, model, plan, costs).ltmc;
    if (next < g - 1e-12 * Math.max(1, Math.abs(g))) {
      rounds.push({ rent: g, count: countRepairs(plan), ltmc: next });
      g = next; rep = plan;
    } else break;
  }
  return { rep, ltmc: g, rounds };
}

/* ---------- simulation ---------- */
function simulateLife(sys, model, x0) {
  let x = x0, t = 0;
  while (sys.phi(x)) {
    const w = pop(x);
    t += -Math.log(1 - Math.random()) / model.nu(w);
    let u = Math.random(), w2 = w - 1;
    if (model.type !== 'exp') {
      let acc = 0; w2 = 0;
      for (let v = w - 1; v >= 0; v--) { acc += model.P(w, v); if (u <= acc) { w2 = v; break; } }
    }
    const up = [];
    for (let i = 0; i < sys.n; i++) if ((x >> i) & 1) up.push(i);
    for (let r = w - w2; r > 0; r--) {
      const j = Math.floor(Math.random() * up.length);
      x &= ~(1 << up[j]); up.splice(j, 1);
    }
  }
  return t;
}
function deathFlip(sys, x0) {
  const up = [];
  for (let i = 0; i < sys.n; i++) if ((x0 >> i) & 1) up.push(i);
  let x = x0, k = 0;
  while (sys.phi(x)) {
    const j = Math.floor(Math.random() * up.length);
    x &= ~(1 << up[j]); up.splice(j, 1); k++;
  }
  return k;
}

if (typeof module !== 'undefined') {
  module.exports = { SYSTEMS, seriesOf, makeModel, analyse, evalPolicy, correctivePolicy, rOutOfN, thresholdPolicy,
    thresholdCurve, optimalPolicy, countRepairs, simulateLife, deathFlip, choose, pop };
}
