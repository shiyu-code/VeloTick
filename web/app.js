const statusEl = document.getElementById('status');
const log = document.getElementById('log');
const rateEl = document.getElementById('rate');
const srvRateEl = document.getElementById('srvRate');
const uptimeEl = document.getElementById('uptime');
const versionEl = document.getElementById('version');
const clientsEl = document.getElementById('clients');
const buffersEl = document.getElementById('buffers');
const lastEl = document.getElementById('lastUpdate');
const pauseEl = document.getElementById('pause');
const filterEl = document.getElementById('filter');
const latencyEl = document.getElementById('latency');
const latencySamples = [];
const maxLatencySamples = 50;

const chartInstEl = document.getElementById('chartInst');
const chartInfoEl = document.getElementById('chartInfo');
const chartCanvas = document.getElementById('chartCanvas');
const chartCtx = chartCanvas ? chartCanvas.getContext('2d') : null;

const kInstEl = document.getElementById('kInst');
const kTfEl = document.getElementById('kTf');
const kInfoEl = document.getElementById('kInfo');
const kCanvas = document.getElementById('kCanvas');
const kCtx = kCanvas.getContext('2d');
const kTooltip = document.getElementById('kTooltip');
const kShowGridEl = document.getElementById('kShowGrid');
const kShowVolEl = document.getElementById('kShowVol');
const kShowMA5El = document.getElementById('kShowMA5');
const kShowMA10El = document.getElementById('kShowMA10');
const kShowToEl = document.getElementById('kShowTo');
const kFollowEl = document.getElementById('kFollow');

let kVisibleN = 120; // visible bars count
let kOffset = 0;     // right offset for panning
let kCross = null;   // crosshair state

let livePaused = false;
let filterText = '';
let ws = null;
let reconnectAttempts = 0;
let backoffMs = 1000;
const maxBackoffMs = 15000;

const latest = new Map();
const history = new Map();
const maxPoints = 300;

const lastVolByInst = new Map();
const lastToByInst = new Map();
const kStore = new Map(); // inst => { tfMs => [ { ts, o, h, l, c, v, to, oi } ] }
const timeframes = [1000, 5000, 15000, 60000, 300000, 900000, 3600000];
const maxKBars = 600;
let tickCount = 0;
let lastRateTs = Date.now();
function updateRate() {
  const now = Date.now();
  const dt = (now - lastRateTs) / 1000;
  if (dt >= 1) {
    const rate = Math.round(tickCount / dt);
    rateEl.textContent = `Rate: ${rate} tps`;
    tickCount = 0;
    lastRateTs = now;
  }
}
setInterval(updateRate, 1000);

function passesFilter(inst) {
  return !filterText || inst.toUpperCase().includes(filterText);
}

function formatDateTimeMs(ts) {
  const d = new Date(ts);
  const pad = (n) => String(n).padStart(2, '0');
  const pad3 = (n) => String(n).padStart(3, '0');
  return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${pad3(d.getMilliseconds())}`;
}

function renderOrUpdateRow(item) {
  if (!passesFilter(item.i)) return;
  const tbody = document.querySelector('#latestTable tbody');
  const id = `row-${item.i}`;
  let tr = document.getElementById(id);
  const dtStr = formatDateTimeMs(item.t);
  const html = `<td>${dtStr}</td><td>${item.i}</td><td>${Number(item.p).toFixed(2)}</td><td>${Number(item.bp||0).toFixed(2)}</td><td>${Number(item.ap||0).toFixed(2)}</td><td>${Number(item.bv||0).toFixed(0)}</td><td>${Number(item.av||0).toFixed(0)}</td><td>${Number(item.v||0).toFixed(0)}</td><td>${Number(item.to||0).toFixed(2)}</td><td>${Number(item.oi||0).toFixed(0)}</td>`;
  if (!tr) {
    tr = document.createElement('tr');
    tr.id = id;
    tr.innerHTML = html;
    tbody.appendChild(tr);
  } else {
    tr.innerHTML = html;
  }
  tr.classList.remove('updated');
  void tr.offsetWidth; // reflow to restart animation
  tr.classList.add('updated');
  tr.onclick = () => { if (chartInstEl) { chartInstEl.value = item.i; drawChart(); } };
}

function renderAllFromCache(sortKey, asc) {
  const tbody = document.querySelector('#latestTable tbody');
  tbody.innerHTML = '';
  const arr = Array.from(latest.values())
    .filter(item => passesFilter(item.i))
    .sort((a, b) => {
      let va = a[sortKey];
      let vb = b[sortKey];
      if (sortKey === 't') { va = Number(va); vb = Number(vb); }
      else if (typeof va === 'string' || typeof vb === 'string') { va = String(va); vb = String(vb); }
      else { va = Number(va); vb = Number(vb); }
      const cmp = va < vb ? -1 : va > vb ? 1 : 0;
      return asc ? cmp : -cmp;
    });
  for (const item of arr) renderOrUpdateRow(item);
}

let sortBy = 'i';
let sortAsc = true;

function attachHeaderSortHandlers() {
  document.querySelectorAll('#latestTable thead th[data-key]').forEach(th => {
    th.style.cursor = 'pointer';
    th.addEventListener('click', () => {
      const key = th.getAttribute('data-key');
      if (sortBy === key) sortAsc = !sortAsc; else { sortBy = key; sortAsc = true; }
      renderAllFromCache(sortBy, sortAsc);
    });
  });
}
attachHeaderSortHandlers();

function setStatus(className, text) {
  statusEl.classList.remove('status-ok', 'status-bad', 'status-warn');
  statusEl.classList.add(className);
  statusEl.querySelector('.text').textContent = text;
}

function updateChartOptions() {
  if (!chartInstEl) return;
  const prev = chartInstEl.value;
  const instruments = Array.from(latest.keys()).sort((a,b)=>a.localeCompare(b));
  chartInstEl.innerHTML = '';
  for (const i of instruments) {
    const opt = document.createElement('option');
    opt.value = i; opt.textContent = i; chartInstEl.appendChild(opt);
  }
  if (!chartInstEl.value && instruments.length > 0) chartInstEl.value = instruments[0];
  if (prev && instruments.includes(prev)) chartInstEl.value = prev;
}

function drawChart() {
  if (!chartCtx || !chartInstEl) return;
  const inst = chartInstEl.value;
  const arr = history.get(inst) || [];
  chartInfoEl.textContent = `Points: ${arr.length}`;
  const W = chartCanvas.width, H = chartCanvas.height;
  chartCtx.clearRect(0, 0, W, H);
  if (arr.length < 2) return;

  // 分时图：时间为横轴，价格为纵轴，显示坐标轴与刻度
  const padL = 50, padR = 12, padT = 12, padB = 28;
  const plotW = W - padL - padR;
  const plotH = H - padT - padB;

  const prices = arr.map(it => Number(it.p));
  let minV = Math.min(...prices), maxV = Math.max(...prices);
  const vPad = (maxV - minV) * 0.05 || 1.0;
  minV -= vPad; maxV += vPad;

  const times = arr.map(it => Number(it.w || it.t));
  const minT = Math.min(...times), maxT = Math.max(...times);

  const toX = (ts) => padL + ((ts - minT) / (maxT - minT)) * plotW;
  const toY = (v) => padT + (1 - (v - minV) / (maxV - minV)) * plotH;

  // 坐标轴
  chartCtx.save();
  chartCtx.strokeStyle = '#999';
  chartCtx.lineWidth = 1;
  // y 轴
  chartCtx.beginPath();
  chartCtx.moveTo(padL, padT);
  chartCtx.lineTo(padL, padT + plotH);
  chartCtx.stroke();
  // x 轴
  chartCtx.beginPath();
  chartCtx.moveTo(padL, padT + plotH);
  chartCtx.lineTo(W - padR, padT + plotH);
  chartCtx.stroke();

  // 刻度与网格
  chartCtx.fillStyle = '#666';
  chartCtx.font = '12px sans-serif';

  // y 轴刻度（价格）
  const yTicks = 4;
  for (let i = 0; i <= yTicks; i++) {
    const val = minV + (i / yTicks) * (maxV - minV);
    const y = toY(val);
    // tick mark
    chartCtx.strokeStyle = '#999';
    chartCtx.beginPath();
    chartCtx.moveTo(padL - 5, y);
    chartCtx.lineTo(padL, y);
    chartCtx.stroke();
    // label
    chartCtx.textAlign = 'right';
    chartCtx.fillText(val.toFixed(2), padL - 8, y + 4);
    // grid line
    chartCtx.strokeStyle = '#eee';
    chartCtx.setLineDash([2, 2]);
    chartCtx.beginPath();
    chartCtx.moveTo(padL, y);
    chartCtx.lineTo(W - padR, y);
    chartCtx.stroke();
    chartCtx.setLineDash([]);
  }

  // x 轴刻度（时间）
  const xTicks = 6;
  for (let i = 0; i <= xTicks; i++) {
    const tVal = minT + (i / xTicks) * (maxT - minT);
    const x = toX(tVal);
    // tick mark
    chartCtx.strokeStyle = '#999';
    chartCtx.beginPath();
    chartCtx.moveTo(x, padT + plotH);
    chartCtx.lineTo(x, padT + plotH + 5);
    chartCtx.stroke();
    // label
    const d = new Date(tVal);
    const label = d.toLocaleTimeString();
    chartCtx.textAlign = 'center';
    chartCtx.fillText(label, x, padT + plotH + 18);
  }

  // 分时价格线
  chartCtx.strokeStyle = '#1f77b4';
  chartCtx.lineWidth = 1.5;
  chartCtx.beginPath();
  for (let i = 0; i < arr.length; i++) {
    const x = toX(times[i]);
    const y = toY(prices[i]);
    if (i === 0) chartCtx.moveTo(x, y); else chartCtx.lineTo(x, y);
  }
  chartCtx.stroke();
  chartCtx.restore();
}
 if (chartInstEl) chartInstEl.addEventListener('change', drawChart);

function updateKInstrOptions() {
  if (!kInstEl) return;
  const prev = kInstEl.value;
  const instruments = Array.from(latest.keys()).sort((a,b)=>a.localeCompare(b));
  kInstEl.innerHTML = '';
  for (const i of instruments) {
    const opt = document.createElement('option');
    opt.value = i; opt.textContent = i; kInstEl.appendChild(opt);
  }
  if (!kInstEl.value && instruments.length > 0) kInstEl.value = instruments[0];
  if (prev && instruments.includes(prev)) kInstEl.value = prev;
}

function drawKline() {
  const inst = kInstEl.value;
  const tf = parseInt(kTfEl.value, 10);
  if (!inst || !kStore.has(inst)) return;
  const tfMap = kStore.get(inst);
  const bars = tfMap.get(tf) || [];
  kInfoEl.textContent = `Bars: ${bars.length}`;
  kCtx.clearRect(0, 0, kCanvas.width, kCanvas.height);
  if (!bars.length) return;

  const total = bars.length;
  const start = Math.max(0, total - kVisibleN - kOffset);
  const end = Math.max(start, total - kOffset);
  const vis = bars.slice(start, end);
  if (!vis.length) return;

  const W = kCanvas.width;
  const H = kCanvas.height;
  const padL = 40, padR = 10, padT = 10, padB = 20;
  const plotW = W - padL - padR;
  const plotH = H - padT - padB;

  // Split areas: price (top ~72%), volume (bottom ~28%)
  const priceTop = padT;
  const priceBottom = padT + Math.round(plotH * 0.72);
  const volTop = priceBottom + 4;
  const volBottom = H - padB;
  const priceH = priceBottom - priceTop;
  const volH = volBottom - volTop;

  // Price range
  let pMin = Infinity, pMax = -Infinity;
  for (const b of vis) { pMin = Math.min(pMin, b.l); pMax = Math.max(pMax, b.h); }
  if (pMin === pMax) { pMin -= 1; pMax += 1; }
  const toX = (i) => padL + (i + 0.5) * (plotW / vis.length);
  const toYPrice = (p) => priceTop + (1 - (p - pMin) / (pMax - pMin)) * priceH;

  // Grid (optional)
  if (kShowGridEl && kShowGridEl.checked) {
    kCtx.save();
    kCtx.strokeStyle = '#eee';
    kCtx.setLineDash([3,3]);
    const hLines = 4;
    for (let i = 0; i <= hLines; i++) {
      const y = priceTop + (i / hLines) * priceH;
      kCtx.beginPath();
      kCtx.moveTo(padL, y);
      kCtx.lineTo(W - padR, y);
      kCtx.stroke();
    }
    const vStep = Math.max(5, Math.floor(vis.length / 6));
    for (let i = 0; i < vis.length; i += vStep) {
      const x = padL + (i + 0.5) * (plotW / vis.length);
      kCtx.beginPath();
      kCtx.moveTo(x, priceTop);
      kCtx.lineTo(x, priceBottom);
      kCtx.stroke();
    }
    kCtx.setLineDash([]);
    kCtx.restore();
  }

  // Axes + labels
  kCtx.save();
  kCtx.strokeStyle = '#999';
  kCtx.beginPath();
  kCtx.moveTo(padL, priceTop);
  kCtx.lineTo(padL, priceBottom);
  kCtx.moveTo(padL, volBottom);
  kCtx.lineTo(W - padR, volBottom);
  kCtx.stroke();
  kCtx.fillStyle = '#666';
  kCtx.font = '12px sans-serif';
  kCtx.textAlign = 'right';
  kCtx.fillText(pMax.toFixed(2), padL - 6, priceTop + 12);
  kCtx.fillText(pMin.toFixed(2), padL - 6, priceBottom);
  // bottom time labels (max ~6)
  kCtx.textAlign = 'center';
  const vStepLabels = Math.max(5, Math.floor(vis.length / 6));
  for (let i = 0; i < vis.length; i += vStepLabels) {
    const x = padL + (i + 0.5) * (plotW / vis.length);
    const t = new Date(vis[i].ts);
    const label = t.toLocaleTimeString();
    kCtx.fillText(label, x, volBottom + 14);
  }
  kCtx.restore();

  // Candles
  const candleW = Math.max(3, Math.floor(plotW / vis.length * 0.6));
  for (let i = 0; i < vis.length; i++) {
    const b = vis[i];
    const x = toX(i);
    const yOpen = toYPrice(b.o);
    const yClose = toYPrice(b.c);
    const yHigh = toYPrice(b.h);
    const yLow = toYPrice(b.l);
    const up = b.c >= b.o;
    const color = up ? '#2ecc71' : '#e74c3c';
    kCtx.strokeStyle = color;
    kCtx.lineWidth = 1;
    // wick
    kCtx.beginPath();
    kCtx.moveTo(x, yHigh);
    kCtx.lineTo(x, yLow);
    kCtx.stroke();
    // body
    const bodyH = Math.max(1, Math.abs(yClose - yOpen));
    const bodyY = Math.min(yClose, yOpen);
    kCtx.fillStyle = color;
    kCtx.fillRect(x - candleW/2, bodyY, candleW, bodyH);
  }

  // MA5 (optional)
  if (kShowMA5El && kShowMA5El.checked) {
    kCtx.save();
    kCtx.strokeStyle = '#3498db';
    kCtx.lineWidth = 2;
    kCtx.beginPath();
    for (let i = 0; i < vis.length; i++) {
      const startIdx = Math.max(0, i - 4);
      const len = i - startIdx + 1;
      let sum = 0;
      for (let j = startIdx; j <= i; j++) sum += vis[j].c;
      const ma = sum / len;
      const x = toX(i);
      const y = toYPrice(ma);
      if (i === 0) kCtx.moveTo(x, y); else kCtx.lineTo(x, y);
    }
    kCtx.stroke();
    kCtx.restore();
  }

  // MA10 (optional)
  if (kShowMA10El && kShowMA10El.checked) {
    kCtx.save();
    kCtx.strokeStyle = '#f1c40f';
    kCtx.lineWidth = 2;
    kCtx.beginPath();
    for (let i = 0; i < vis.length; i++) {
      const startIdx = Math.max(0, i - 9);
      const len = i - startIdx + 1;
      let sum = 0;
      for (let j = startIdx; j <= i; j++) sum += vis[j].c;
      const ma = sum / len;
      const x = toX(i);
      const y = toYPrice(ma);
      if (i === 0) kCtx.moveTo(x, y); else kCtx.lineTo(x, y);
    }
    kCtx.stroke();
    kCtx.restore();
  }

  // Volume & Turnover (optional)
  const drawVol = kShowVolEl && kShowVolEl.checked;
  const drawTo = kShowToEl && kShowToEl.checked;
  if (drawVol || drawTo) {
    let volMax = 0, toMax = 0;
    if (drawVol) { for (const b of vis) volMax = Math.max(volMax, b.v || 0); }
    if (drawTo) { for (const b of vis) toMax = Math.max(toMax, b.to || 0); }
    const toYVol = (v) => volBottom - (volMax ? (v / volMax) * volH : 0);
    const toYTo  = (v) => volBottom - (toMax ? (v / toMax) * volH : 0);
    const gap = Math.max(1, Math.floor((plotW / vis.length) * 0.05));
    const baseW = Math.max(2, Math.floor(plotW / vis.length) - gap);

    // dual-axis ticks & labels for subchart
    kCtx.save();
    kCtx.fillStyle = '#666';
    kCtx.strokeStyle = '#999';
    kCtx.font = '12px sans-serif';
    const subTicks = 4;
    const fmtAxis = (v) => {
      const abs = Math.abs(v);
      const trim = (s) => s.replace(/\.0+$/, '').replace(/(\.\d*[1-9])0+$/, '$1');
      if (abs >= 1e8) return trim((v / 1e8).toFixed(2)) + '亿';
      if (abs >= 1e4) return trim((v / 1e4).toFixed(2)) + '万';
      if (abs >= 1000) return Number(v.toFixed(0)).toLocaleString();
      return String(Math.round(v));
    };
    // grid lines across subchart (optional)
    if (kShowGridEl && kShowGridEl.checked) {
      kCtx.save();
      kCtx.strokeStyle = '#eee';
      kCtx.setLineDash([2,2]);
      for (let i = 0; i <= subTicks; i++) {
        const y = volBottom - (i / subTicks) * volH;
        kCtx.beginPath();
        kCtx.moveTo(padL, y);
        kCtx.lineTo(W - padR, y);
        kCtx.stroke();
      }
      kCtx.setLineDash([]);
      kCtx.restore();
    }
    // draw Y-axes lines for subchart
    if (drawVol) {
      kCtx.beginPath();
      kCtx.moveTo(padL, volTop);
      kCtx.lineTo(padL, volBottom);
      kCtx.stroke();
    }
    if (drawTo) {
      kCtx.beginPath();
      kCtx.moveTo(W - padR, volTop);
      kCtx.lineTo(W - padR, volBottom);
      kCtx.stroke();
    }
    if (drawVol) {
      kCtx.textAlign = 'right';
      for (let i = 0; i <= subTicks; i++) {
        const val = (i / subTicks) * volMax;
        const y = toYVol(val);
        // tick
        kCtx.beginPath();
        kCtx.moveTo(padL - 5, y);
        kCtx.lineTo(padL, y);
        kCtx.stroke();
        // label
        kCtx.fillText(fmtAxis(val), padL - 6, y + 4);
      }
    }
    if (drawTo) {
      kCtx.textAlign = 'left';
      for (let i = 0; i <= subTicks; i++) {
        const val = (i / subTicks) * toMax;
        const y = toYTo(val);
        // tick
        kCtx.beginPath();
        kCtx.moveTo(W - padR, y);
        kCtx.lineTo(W - padR + 5, y);
        kCtx.stroke();
        // label
        kCtx.fillText(fmtAxis(val), W - padR + 6, y + 4);
      }
    }
    kCtx.restore();

    if (drawVol) {
      const vW = baseW;
      for (let i = 0; i < vis.length; i++) {
        const b = vis[i];
        const xCenter = toX(i);
        const xLeft = Math.floor(xCenter - vW/2);
        const yTop = toYVol(b.v || 0);
        const up = b.c >= b.o;
        kCtx.fillStyle = up ? 'rgba(46,204,113,0.45)' : 'rgba(231,76,60,0.45)';
        kCtx.fillRect(xLeft, yTop, vW, volBottom - yTop);
      }
    }

    if (drawTo) {
      const toW = Math.max(2, Math.floor(baseW * 0.6));
      for (let i = 0; i < vis.length; i++) {
        const b = vis[i];
        const xCenter = toX(i);
        const xLeft = Math.floor(xCenter - toW/2);
        const yTop = toYTo(b.to || 0);
        kCtx.fillStyle = 'rgba(255,165,0,0.45)'; // orange for Turnover
        kCtx.fillRect(xLeft, yTop, toW, volBottom - yTop);
      }
    }
  }
  kCtx.restore();
}
[kShowGridEl, kShowVolEl, kShowToEl, kShowMA5El, kShowMA10El].forEach(el => el && el.addEventListener('change', drawKline));
if (kTfEl) kTfEl.addEventListener('change', drawKline);

function appendK(inst, tfms, tWall, price, volDelta, turnoverDelta, oiSnap) {
  let instObj = kStore.get(inst);
  if (!instObj) { instObj = new Map(); kStore.set(inst, instObj); }
  const bucket = Math.floor(tWall / tfms) * tfms;
  let bars = instObj.get(tfms);
  if (!bars) { bars = []; instObj.set(tfms, bars); }
  const last = bars[bars.length - 1];
  if (!last || last.ts !== bucket) {
    // open new bar
    bars.push({ ts: bucket, o: price, h: price, l: price, c: price, v: Math.max(0, volDelta||0), to: Math.max(0, turnoverDelta||0), oi: Number(oiSnap||0) });
    if (bars.length > maxKBars) bars.shift();
  } else {
    last.h = Math.max(last.h, price);
    last.l = Math.min(last.l, price);
    last.c = price;
    last.v += Math.max(0, volDelta||0);
    last.to = (last.to||0) + Math.max(0, turnoverDelta||0);
    last.oi = Number(oiSnap||0);
  }
}

function connectWs() {
  if (ws) { try { ws.close(); } catch (e) {} ws = null; }
  setStatus('status-warn', 'Connecting...');
  ws = new WebSocket(`ws://${location.host}`);
  ws.onopen = () => {
    reconnectAttempts = 0;
    backoffMs = 1000;
    setStatus('status-ok', 'Connected');
  };
  ws.onclose = () => {
    reconnectAttempts++;
    const delay = Math.min(backoffMs * Math.pow(2, reconnectAttempts - 1), maxBackoffMs);
    setStatus('status-bad', `Closed (retry ${Math.round(delay/1000)}s, attempt ${reconnectAttempts})`);
    setTimeout(connectWs, delay);
  };
  ws.onmessage = (ev) => {
    if (livePaused) return;
    tickCount++;
    lastEl.textContent = `Last: ${new Date().toLocaleTimeString()}`;
    try {
      const item = JSON.parse(ev.data);
      latest.set(item.i, item);
      const tWall = item.w ? Number(item.w) : Date.now();
      if (item.w) {
        const d = Math.max(0, Date.now() - tWall);
        latencySamples.push(d);
        if (latencySamples.length > maxLatencySamples) latencySamples.shift();
        const avg = Math.round(latencySamples.reduce((a,b)=>a+b,0)/latencySamples.length);
        const maxD = Math.max(...latencySamples);
        latencyEl.textContent = `Delay: ${d} ms (avg ${avg}, max ${maxD})`;
      } else {
        latencyEl.textContent = `Delay: n/a`;
      }
      let arr = history.get(item.i);
      if (!arr) { arr = []; history.set(item.i, arr); updateChartOptions(); updateKInstrOptions(); if (chartInstEl && !chartInstEl.value) chartInstEl.value = item.i; if (kInstEl && !kInstEl.value) kInstEl.value = item.i; }
      arr.push({ p: Number(item.p), MA5: Number(item.MA5||item.ma5||0), t: Number(item.t), w: tWall });
      if (arr.length > maxPoints) arr.shift();
      const prevVol = lastVolByInst.get(item.i) || 0;
      const curVol = Number(item.v||0);
      let volDelta = curVol - prevVol;
      if (volDelta < 0) volDelta = curVol; // support non-cumulative v
      volDelta = Math.max(0, volDelta);
      lastVolByInst.set(item.i, curVol);
 
      const prevTo = lastToByInst.get(item.i) || 0;
      const curTo = Number(item.to||0);
      let toDelta = curTo - prevTo;
      if (toDelta < 0) toDelta = curTo; // support non-cumulative to
      toDelta = Math.max(0, toDelta);
      lastToByInst.set(item.i, curTo);
 
      const oiSnap = Number(item.oi||0);
      for (const tfms of timeframes) appendK(item.i, tfms, tWall, Number(item.p), volDelta, toDelta, oiSnap);
      renderOrUpdateRow(item);
      if (chartInstEl && chartInstEl.value === item.i) drawChart();
      if (kInstEl && kInstEl.value === item.i) { if (kFollowEl && kFollowEl.checked) kOffset = 0; drawKline(); }
    } catch (e) {
      const line = ev.data + '\n';
      log.textContent = line + log.textContent;
      if (log.textContent.length > 65536) {
        log.textContent = log.textContent.substring(0, 65536);
      }
    }
  };
}

connectWs();

async function pollStats() {
  try {
    const resp = await fetch('/stats');
    const s = await resp.json();
    clientsEl.textContent = `Clients: ${s.clients}`;
    buffersEl.textContent = `Raw: ${s.rawSize} | Clean: ${s.cleanSize}`;
    if (srvRateEl && typeof s.tps === 'number') {
      srvRateEl.textContent = `Srv: ${s.tps} tps`;
    }
    if (uptimeEl && typeof s.uptimeMs === 'number') {
      const sec = Math.floor(s.uptimeMs / 1000);
      uptimeEl.textContent = `Uptime: ${sec}s`;
    }
  } catch (e) {
    // ignore
  }
}
setInterval(pollStats, 2000);
pollStats();

// 新增：轮询 /health 以展示服务器版本与健康状态
async function pollHealth() {
  try {
    const resp = await fetch('/health');
    const h = await resp.json();
    if (versionEl && typeof h.version === 'string') {
      versionEl.textContent = `SrvVer: ${h.version}`;
    }
    // 如果需要可以根据 h.status 展示额外提示，这里保持简洁
  } catch (e) {
    // ignore
  }
}
setInterval(pollHealth, 5000);
pollHealth();

async function refreshLatest() {
  try {
    const resp = await fetch('/api/ticks/latest');
    const arr = await resp.json();
    const tbody = document.querySelector('#latestTable tbody');
    tbody.innerHTML = '';
    arr
      .filter(item => passesFilter(item.i))
      .sort((a,b)=>a.i.localeCompare(b.i))
      .forEach(item => {
        latest.set(item.i, item);
        const tr = document.createElement('tr');
        const dtStr = formatDateTimeMs(item.t);
        tr.innerHTML = `<td>${dtStr}</td><td>${item.i}</td><td>${Number(item.p).toFixed(2)}</td><td>${Number(item.bp||0).toFixed(2)}</td><td>${Number(item.ap||0).toFixed(2)}</td><td>${Number(item.bv||0).toFixed(0)}</td><td>${Number(item.av||0).toFixed(0)}</td><td>${Number(item.v||0).toFixed(0)}</td><td>${Number(item.to||0).toFixed(2)}</td><td>${Number(item.oi||0).toFixed(0)}</td>`;
        tbody.appendChild(tr);
      });
    renderAllFromCache(sortBy, sortAsc);
  } catch (e) {
    const tbody = document.querySelector('#latestTable tbody');
    tbody.innerHTML = `<tr><td colspan=4>Error: ${e}</td></tr>`;
  }
}

document.getElementById('refresh').onclick = refreshLatest;
  let timer = null;
  document.getElementById('auto').onchange = (e) => {
    if (e.target.checked) {
      refreshLatest();
      timer = setInterval(() => { refreshLatest(); drawKline(); }, 2000);
    } else {
      if (timer) clearInterval(timer);
      timer = null;
    }
  };

pauseEl.onchange = (e) => { livePaused = !!e.target.checked; };
filterEl.addEventListener('input', (e) => {
  filterText = e.target.value.trim().toUpperCase();
  renderAllFromCache(sortBy, sortAsc);
});

(function bindKlineInteractions(){
  let dragging = false;
  let dragStartX = 0;
  let dragStartOffset = 0;

  kCanvas.addEventListener('mousemove', (e) => {
    const rect = kCanvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const inst = kInstEl.value;
    const tf = parseInt(kTfEl.value, 10);
    if (!inst || !kStore.has(inst)) return;
    const bars = (kStore.get(inst).get(tf) || []);
    const total = bars.length;
    const start = Math.max(0, total - kVisibleN - kOffset);
    const end = Math.max(start, total - kOffset);
    const vis = bars.slice(start, end);
    if (!vis.length) return;
    const W = kCanvas.width;
    const H = kCanvas.height;
    const padL = 40, padR = 10, padT = 10, padB = 20;
    const plotW = W - padL - padR;
    const iFloat = (x - padL) / (plotW / vis.length) - 0.5;
    const idx = Math.min(vis.length - 1, Math.max(0, Math.round(iFloat)));
    kCross = { x, y, idx };
    // Tooltip
    const b = vis[idx];
    const dateStr = new Date(b.ts).toLocaleString();
    kTooltip.innerHTML = `
      <div>${dateStr}</div>
      <div>O:${b.o} H:${b.h} L:${b.l} C:${b.c} V:${Math.round(b.v)} TO:${Math.round(b.to||0)} OI:${Math.round(b.oi||0)}</div>
    `;
    const ttW = 220, ttH = 44;
    let tx = x + 12, ty = y + 12;
    if (tx + ttW > W) tx = x - ttW - 12;
    if (ty + ttH > H) ty = y - ttH - 12;
    kTooltip.style.left = `${tx}px`;
    kTooltip.style.top = `${ty}px`;
    kTooltip.style.display = 'block';
    drawKline();
  });
  kCanvas.addEventListener('mouseleave', () => {
    kCross = null;
    kTooltip.style.display = 'none';
    drawKline();
  });

  kCanvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const delta = Math.sign(e.deltaY);
    const old = kVisibleN;
    kVisibleN = Math.min(300, Math.max(20, kVisibleN + delta * 10));
    // keep right edge consistent by adjusting offset
    if (dragging === false) {
      const inst = kInstEl.value;
      const tf = parseInt(kTfEl.value, 10);
      const bars = (kStore.get(inst)?.get(tf) || []);
      const maxOffset = Math.max(0, bars.length - kVisibleN);
      kOffset = Math.min(kOffset, maxOffset);
    }
    drawKline();
  }, { passive: false });

  kCanvas.addEventListener('mousedown', (e) => {
    dragging = true;
    dragStartX = e.clientX;
    dragStartOffset = kOffset;
  });
  window.addEventListener('mouseup', () => { dragging = false; });
  window.addEventListener('mousemove', (e) => {
    if (!dragging) return;
    const rect = kCanvas.getBoundingClientRect();
    const W = kCanvas.width;
    const padL = 40, padR = 10;
    const inst = kInstEl.value;
    const tf = parseInt(kTfEl.value, 10);
    const bars = (kStore.get(inst)?.get(tf) || []);
    const maxOffset = Math.max(0, bars.length - kVisibleN);
    const dx = e.clientX - dragStartX;
    const perBar = (W - padL - padR) / kVisibleN;
    const moveBars = Math.round(dx / perBar);
    kOffset = Math.min(maxOffset, Math.max(0, dragStartOffset - moveBars));
    drawKline();
  });
})();
// Reset pan/zoom on instrument/timeframe change
kInstEl.addEventListener('change', () => { kOffset = 0; drawKline(); });
kTfEl.addEventListener('change', () => { kOffset = 0; drawKline(); });
[kShowGridEl, kShowVolEl, kShowToEl, kShowMA5El, kShowMA10El].forEach(el => el && el.addEventListener('change', drawKline));
if (kFollowEl) kFollowEl.addEventListener('change', () => { if (kFollowEl.checked) kOffset = 0; drawKline(); });